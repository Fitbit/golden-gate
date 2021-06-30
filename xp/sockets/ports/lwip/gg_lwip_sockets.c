/**
 *
 * @file
 *
 * @copyright
 * Copyright 2017-2020 Fitbit, Inc
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author Gilles Boccon-Gibod
 *
 * @date 2017-11-13
 *
 * @details
 *
 * LWIP API implementation of the socket interface.
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "lwip/udp.h"
#include "lwip/ip_addr.h"

#include "xp/annotations/gg_annotations.h"
#include "xp/common/gg_port.h"
#include "xp/common/gg_io.h"
#include "xp/common/gg_types.h"
#include "xp/common/gg_memory.h"
#include "xp/common/gg_logging.h"
#include "xp/common/gg_timer.h"
#include "xp/common/gg_threads.h"
#include "xp/sockets/gg_sockets.h"
#include "gg_lwip_sockets.h"

/*----------------------------------------------------------------------
|   logging
+---------------------------------------------------------------------*/
GG_SET_LOCAL_LOGGER("gg.xp.sockets.lwip")

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/
#define GG_LWIP_SOCKETS_MAX_DATAGRAM_SIZE             65536
#define GG_LWIP_DATAGRAM_SOCKET_MAX_RESEND_SLEEP_TIME 128   // milliseconds
#define GG_LWIP_DATAGRAM_SOCKET_MIN_RESEND_SLEEP_TIME 8     // milliseconds

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
typedef struct {
    GG_IMPLEMENTS(GG_DatagramSocket);
    GG_IMPLEMENTS(GG_DataSink);
    GG_IMPLEMENTS(GG_DataSource);
    GG_IMPLEMENTS(GG_DataSinkListener);
    GG_IMPLEMENTS(GG_TimerListener);

    struct udp_pcb*      udp_socket;
    GG_DataSink*         data_sink;
    bool                 connected;
    unsigned int         max_datagram_size;
    GG_SocketAddress     local_address;
    GG_SocketAddress     remote_address;
    GG_DataSinkListener* sink_listener;
    GG_TimerScheduler*   timer_scheduler;
    GG_Timer*            resend_timer;
    uint32_t             resend_sleep_time;

    GG_THREAD_GUARD_ENABLE_BINDING
} GG_LwipDatagramSocket;

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

//----------------------------------------------------------------------
static void
IpAddressToLwipIpAddress(const GG_IpAddress* gg_ip_address,
                         ip_addr_t*          lwip_ip_address)
{
    IP_ADDR4(lwip_ip_address,
             gg_ip_address->ipv4[0],
             gg_ip_address->ipv4[1],
             gg_ip_address->ipv4[2],
             gg_ip_address->ipv4[3]);
}

//----------------------------------------------------------------------
static void
LwipIpAddressToIpAddress(const ip_addr_t* lwip_ip_address,
                         GG_IpAddress*    gg_ip_address)
{
    GG_IpAddress_SetFromInteger(gg_ip_address, lwip_htonl(ip_2_ip4(lwip_ip_address)->addr));
}

//----------------------------------------------------------------------
static GG_Result
MapErrorCode(err_t error)
{
    switch (error) {
        case ERR_OK:
            return GG_SUCCESS;

        case ERR_MEM:
            return GG_ERROR_OUT_OF_MEMORY;

        case ERR_BUF:
            return GG_ERROR_OUT_OF_RESOURCES;

        case ERR_TIMEOUT:
            return GG_ERROR_TIMEOUT;
        
        case ERR_RTE:
            return GG_ERROR_NETWORK_UNREACHABLE;

        case ERR_VAL:
            return GG_ERROR_ILLEGAL_VALUE;

        case ERR_WOULDBLOCK:
            return GG_ERROR_WOULD_BLOCK;

        case ERR_USE:
            return GG_ERROR_ADDRESS_IN_USE;

        case ERR_ALREADY:
            return GG_ERROR_ALREADY_CONNECTING;

        case ERR_ISCONN:
            return GG_ERROR_ALREADY_CONNECTED;

        case ERR_CONN:
            return GG_ERROR_NOT_CONNECTED;

        case ERR_IF:
            return GG_ERROR_LOW_LEVEL_NETIF_ERROR;

        case ERR_ABRT:
            return GG_ERROR_CONNECTION_ABORTED;
        
        case ERR_RST:
            return GG_ERROR_CONNECTION_RESET;

        case ERR_CLSD:
            return GG_ERROR_CONNECTION_CLOSED; 

        case ERR_ARG:
            return GG_ERROR_INVALID_PARAMETERS;

        default:
            GG_LOG_FINER("GG_FAILURE shadowing finer error: %d", (int)error);
            return GG_FAILURE;
    }
}

//----------------------------------------------------------------------
static GG_DataSink*
GG_LwipDatagramSocket_AsDataSink(GG_DatagramSocket* _self)
{
    GG_LwipDatagramSocket* self = GG_SELF(GG_LwipDatagramSocket, GG_DatagramSocket);

    return GG_CAST(self, GG_DataSink);
}

//----------------------------------------------------------------------
static GG_DataSource*
GG_LwipDatagramSocket_AsDataSource(GG_DatagramSocket* _self)
{
    GG_LwipDatagramSocket* self = GG_SELF(GG_LwipDatagramSocket, GG_DatagramSocket);

    return GG_CAST(self, GG_DataSource);
}

//----------------------------------------------------------------------
static void
GG_LwipDatagramSocket_Destroy(GG_DatagramSocket* _self)
{
    if (!_self) return;
    GG_LwipDatagramSocket* self = GG_SELF(GG_LwipDatagramSocket, GG_DatagramSocket);
    GG_THREAD_GUARD_CHECK_BINDING(self);

    // destroy the resend timer if we have one
    GG_Timer_Destroy(self->resend_timer);

    // close the socket
    if (self->udp_socket) {
        udp_remove(self->udp_socket);
    }

    // free the memory
    GG_ClearAndFreeObject(self, 5);
}

//----------------------------------------------------------------------
static GG_Result
GG_LwipDatagramSocket_Attach(GG_DatagramSocket* _self, GG_Loop* loop)
{
    GG_ASSERT(_self);
    GG_LwipDatagramSocket* self = GG_SELF(GG_LwipDatagramSocket, GG_DatagramSocket);
    GG_THREAD_GUARD_CHECK_BINDING(self);

    // check that we're not already attached
    if (self->timer_scheduler) {
        return GG_ERROR_INVALID_STATE;
    }

    // we just need the timer scheduler from the loop
    self->timer_scheduler = GG_Loop_GetTimerScheduler(loop);

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
static void
GG_LwipDatagramSocket_OnDataReceived(void*            _self,
                                     struct udp_pcb*  udp_socket,
                                     struct pbuf*     data,
                                     const ip_addr_t* source_address,
                                     u16_t            source_port)
{
    GG_LwipDatagramSocket* self = (GG_LwipDatagramSocket*)_self;
    GG_COMPILER_UNUSED(udp_socket);

    // check that we have a sink
    if (self->data_sink == NULL) {
        pbuf_free(data);
        return;
    }

    // setup the metadata
    GG_SocketAddressMetadata metadata = GG_SOURCE_SOCKET_ADDRESS_METADATA_INITIALIZER(
        GG_IP_ADDRESS_NULL_INITIALIZER, source_port);
    LwipIpAddressToIpAddress(source_address, &metadata.socket_address.address);

    // convert the data to a gg buffer
    GG_DynamicBuffer* buffer;
    GG_Result result = GG_DynamicBuffer_Create(data->tot_len, &buffer);
    if (GG_FAILED(result)) {
        GG_LOG_WARNING("failed to allocate buffer (%d)", result);
        GG_LOG_COMMS_ERROR(GG_LIB_IP_DATA_DROPPED);

        pbuf_free(data);
        return;
    }
    GG_DynamicBuffer_SetDataSize(buffer, data->tot_len);
    pbuf_copy_partial(data, GG_DynamicBuffer_UseData(buffer), data->tot_len, 0);
    pbuf_free(data);

    // try to send
    // NOTE: there's nothing we can really do here if this fails, as this implementation doesn't
    // maintain a buffer queue
    GG_DataSink_PutData(self->data_sink, GG_DynamicBuffer_AsBuffer(buffer), &metadata.base);
    GG_DynamicBuffer_Release(buffer);
}

//----------------------------------------------------------------------
static GG_Result
GG_LwipDatagramSocket_PutData(GG_DataSink*             _self,
                              GG_Buffer*               data,
                              const GG_BufferMetadata* metadata)
{
    GG_ASSERT(data);
    GG_LwipDatagramSocket* self = GG_SELF(GG_LwipDatagramSocket, GG_DataSink);
    GG_Result gg_result;
    GG_THREAD_GUARD_CHECK_BINDING(self);

    // check that the buffer isn't too large
    if (GG_Buffer_GetDataSize(data) > self->max_datagram_size) {
        return GG_ERROR_INVALID_PARAMETERS;
    }

    // convert the data into an LWIP buffer
    u16_t buffer_size = (u16_t)GG_Buffer_GetDataSize(data);
    struct pbuf* buffer = pbuf_alloc(PBUF_TRANSPORT, buffer_size, PBUF_RAM);
    if (buffer == NULL) {
        GG_LOG_WARNING("pbuf_alloc returned NULL");
        GG_LOG_COMMS_ERROR_CODE(GG_LIB_IP_SEND_FAILED, GG_ERROR_OUT_OF_MEMORY);

        return GG_ERROR_OUT_OF_MEMORY;
    }
    pbuf_take(buffer, GG_Buffer_GetData(data), buffer_size);

    // decide where to send
    err_t result;
    if (self->connected) {
        // try to send the payload
        result = udp_send(self->udp_socket, buffer);
        GG_LOG_FINER("udp_send returned %d", (int)result);
    } else {
        ip_addr_t destination_address;
        u16_t     destination_port;
        if (metadata && metadata->type == GG_BUFFER_METADATA_TYPE_DESTINATION_SOCKET_ADDRESS) {
            const GG_SocketAddressMetadata* socket_metadata = (const GG_SocketAddressMetadata*)metadata;
            IpAddressToLwipIpAddress(&socket_metadata->socket_address.address, &destination_address);
            destination_port = socket_metadata->socket_address.port;
        } else if (self->remote_address.port) {
            IpAddressToLwipIpAddress(&self->remote_address.address, &destination_address);
            destination_port = self->remote_address.port;
        } else {
            pbuf_free(buffer);
            return GG_ERROR_INVALID_STATE;
        }

        // try to send the payload
        result = udp_sendto(self->udp_socket, buffer, &destination_address, destination_port);
        GG_LOG_FINER("udp_sendto returned %d", (int)result);
    }

    pbuf_free(buffer);

    if (result == ERR_OK) {
        // cancel any active resend timer and reset the exponential back-off counter
        self->resend_sleep_time = 0;
        if (self->resend_timer) {
            GG_LOG_FINEST("packet sent, canceling resend timer");
            GG_Timer_Unschedule(self->resend_timer);
        }
    } else {
        if (result == ERR_MEM || result == ERR_WOULDBLOCK) {
            // the packet wasn't accepted by the network interface

            // first, create a timer if we don't already have one.
            // (since this is a rare condition, we don't create the timer upfront)
            if (self->resend_timer == NULL) {
                // check that we've been attached and have a timer scheduler
                if (self->timer_scheduler == NULL) {
                    return GG_ERROR_INVALID_STATE;
                }

                // create the timer
                gg_result = GG_TimerScheduler_CreateTimer(self->timer_scheduler, &self->resend_timer);
                if (GG_FAILED(gg_result)) {
                    return gg_result;
                }
            }

            // schedule to be called back in a short while.
            self->resend_sleep_time = GG_MAX(self->resend_sleep_time, GG_LWIP_DATAGRAM_SOCKET_MIN_RESEND_SLEEP_TIME);
            GG_LOG_FINEST("scheduling UDP resend timer for %u ms", (int)self->resend_sleep_time);
            gg_result = GG_Timer_Schedule(self->resend_timer,
                                          GG_CAST(self, GG_TimerListener),
                                          self->resend_sleep_time);
            if (GG_SUCCEEDED(gg_result)) {
                // let the caller know they should try again later
                return GG_ERROR_WOULD_BLOCK;
            }
        }

        gg_result = MapErrorCode(result);

        GG_LOG_COMMS_ERROR_CODE(GG_LIB_IP_SEND_FAILED, gg_result);

        return gg_result;
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
static GG_Result
GG_LwipDatagramSocket_SetListener(GG_DataSink* _self, GG_DataSinkListener* listener)
{
    GG_LwipDatagramSocket* self = GG_SELF(GG_LwipDatagramSocket, GG_DataSink);
    GG_THREAD_GUARD_CHECK_BINDING(self);

    self->sink_listener = listener;

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
static GG_Result
GG_LwipDatagramSocket_SetDataSink(GG_DataSource* _self, GG_DataSink* data_sink)
{
    GG_LwipDatagramSocket* self = GG_SELF(GG_LwipDatagramSocket, GG_DataSource);
    GG_THREAD_GUARD_CHECK_BINDING(self);

    // keep a reference to the sink
    self->data_sink = data_sink;

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
static void
GG_LwipDatagramSocket_OnCanPut(GG_DataSinkListener* _self)
{
    // nothing to do, in this implementation we don't keep a pending buffer queue
    GG_COMPILER_UNUSED(_self);
}

//----------------------------------------------------------------------
static void
GG_LwipDatagramSocket_OnTimerFired(GG_TimerListener* _self, GG_Timer* timer, uint32_t elapsed)
{
    GG_COMPILER_UNUSED(timer);
    GG_COMPILER_UNUSED(elapsed);
    GG_LwipDatagramSocket* self = GG_SELF(GG_LwipDatagramSocket, GG_TimerListener);
    GG_THREAD_GUARD_CHECK_BINDING(self);

    GG_LOG_FINEST("resend timer fired");

    // adjust the timer based on a capped exponential back-off
    self->resend_sleep_time = GG_MIN(2 * self->resend_sleep_time, GG_LWIP_DATAGRAM_SOCKET_MAX_RESEND_SLEEP_TIME);

    // notify our listener that they can try to put again
    if (self->sink_listener) {
        GG_DataSinkListener_OnCanPut(self->sink_listener);
    }
}

/*----------------------------------------------------------------------
|   function tables
+---------------------------------------------------------------------*/
GG_IMPLEMENT_INTERFACE(GG_LwipDatagramSocket, GG_DatagramSocket) {
    GG_LwipDatagramSocket_AsDataSink,
    GG_LwipDatagramSocket_AsDataSource,
    GG_LwipDatagramSocket_Destroy,
    GG_LwipDatagramSocket_Attach
};

GG_IMPLEMENT_INTERFACE(GG_LwipDatagramSocket, GG_DataSink) {
    GG_LwipDatagramSocket_PutData,
    GG_LwipDatagramSocket_SetListener
};

GG_IMPLEMENT_INTERFACE(GG_LwipDatagramSocket, GG_DataSource) {
    GG_LwipDatagramSocket_SetDataSink
};

GG_IMPLEMENT_INTERFACE(GG_LwipDatagramSocket, GG_DataSinkListener) {
    GG_LwipDatagramSocket_OnCanPut
};

GG_IMPLEMENT_INTERFACE(GG_LwipDatagramSocket, GG_TimerListener) {
    GG_LwipDatagramSocket_OnTimerFired
};

//----------------------------------------------------------------------
GG_Result
GG_LwipDatagramSocket_Create(const GG_SocketAddress* local_address,
                             const GG_SocketAddress* remote_address,
                             bool                    connect_to_remote,
                             unsigned int            max_datagram_size,
                             GG_DatagramSocket**     socket_object)
{
    // default return value
    *socket_object = NULL;

    // check parameters
    if (max_datagram_size > GG_LWIP_SOCKETS_MAX_DATAGRAM_SIZE) {
        return GG_ERROR_INVALID_PARAMETERS;
    }

    // create a UDP socket
    struct udp_pcb* udp_socket = udp_new();
    if (udp_socket == NULL) {
        return GG_ERROR_OUT_OF_MEMORY;
    }

    // bind to the local address
    if (local_address) {
        ip_addr_t bind_address;
        IpAddressToLwipIpAddress(&local_address->address, &bind_address);
        err_t result = udp_bind(udp_socket, &bind_address, local_address->port);
        if (result != ERR_OK) {
            GG_LOG_WARNING("udp_bind failed (%d)", result);
            udp_remove(udp_socket);
            return MapErrorCode(result);
        }
    }

    // connect to the remote address if specified
    if (remote_address && connect_to_remote) {
        ip_addr_t connect_address;
        IpAddressToLwipIpAddress(&remote_address->address, &connect_address);
        err_t result = udp_connect(udp_socket, &connect_address, remote_address->port);
        if (result != ERR_OK) {
            GG_LOG_WARNING("udp_connect failed (%d)", result);
            GG_LOG_COMMS_ERROR_CODE(GG_LIB_IP_CONNECT_FAILED, MapErrorCode(result));

            udp_remove(udp_socket);
            return MapErrorCode(result);
        }
    }

    // TEMP: alloc from heap
    GG_LwipDatagramSocket* self = (GG_LwipDatagramSocket*)GG_AllocateZeroMemory(sizeof(GG_LwipDatagramSocket));
    if (self == NULL) {
        udp_remove(udp_socket);
        return GG_ERROR_OUT_OF_MEMORY;
    }

    // init the instance
    self->udp_socket        = udp_socket;
    self->max_datagram_size = max_datagram_size;
    if (local_address) {
        self->local_address = *local_address;
    }
    if (remote_address) {
        self->remote_address = *remote_address;
    }
    self->connected = connect_to_remote;

    // setup the callback function for when data is received
    udp_recv(udp_socket, GG_LwipDatagramSocket_OnDataReceived, self);

    // init the function tables
    GG_SET_INTERFACE(self, GG_LwipDatagramSocket, GG_DatagramSocket);
    GG_SET_INTERFACE(self, GG_LwipDatagramSocket, GG_DataSink);
    GG_SET_INTERFACE(self, GG_LwipDatagramSocket, GG_DataSource);
    GG_SET_INTERFACE(self, GG_LwipDatagramSocket, GG_DataSinkListener);
    GG_SET_INTERFACE(self, GG_LwipDatagramSocket, GG_TimerListener);

    // bind to the current thread
    GG_THREAD_GUARD_BIND(self);

    // return the object
    *socket_object = GG_CAST(self, GG_DatagramSocket);

    return GG_SUCCESS;
}

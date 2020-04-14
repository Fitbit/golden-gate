/**
 *
 * @file
 *
 * @copyright
 * Copyright 2017 by Fitbit, Inc., all rights reserved.
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

#include "lwip/api.h"
#include "lwip/ip_addr.h"
#include "lwip/opt.h"

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
#define GG_LWIP_SOCKETS_MAX_DATAGRAM_SIZE        65536
#define GG_LWIP_SOCKETS_RESEND_SLEEP_TIME_MS     100

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
typedef struct {
    GG_IMPLEMENTS(GG_DatagramSocket);
    GG_IMPLEMENTS(GG_DataSink);
    GG_IMPLEMENTS(GG_DataSource);
    GG_IMPLEMENTS(GG_DataSinkListener);
    GG_IMPLEMENTS(GG_TimerListener);

    struct netconn*      udp_socket;
    GG_DataSink*         data_sink;
    bool                 connected;
    unsigned int         max_datagram_size;
    GG_SocketAddress     local_address;
    GG_SocketAddress     remote_address;
    GG_DataSinkListener* sink_listener;
    GG_Loop*             loop;
    GG_Timer*            resend_timer;

    GG_THREAD_GUARD_ENABLE_BINDING
} GG_LwipDatagramSocket;

/*----------------------------------------------------------------------
|   globals
+---------------------------------------------------------------------*/

// Pre-allocate memory for socket objects because we need to look them
// up while in an LWIP callback (which unfortunately don't have a way
// to carry a context pointer).
static GG_LwipDatagramSocket GG_LwipSockets[MEMP_NUM_UDP_PCB];

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

        case ERR_WOULDBLOCK:
            return GG_ERROR_WOULD_BLOCK;

        case ERR_TIMEOUT:
            return GG_ERROR_TIMEOUT;

        case ERR_ARG:
            return GG_ERROR_INVALID_PARAMETERS;

        case ERR_RTE:
            return GG_ERROR_NETWORK_UNREACHABLE;

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
    GG_LwipDatagramSocket* self = GG_SELF(GG_LwipDatagramSocket, GG_DatagramSocket);
    GG_THREAD_GUARD_CHECK_BINDING(self);

    // destroy the resend timer if we have one
    GG_Timer_Destroy(self->resend_timer);

    // close the socket
    if (self->udp_socket) {
        netconn_delete(self->udp_socket);
    }

    // zero out the socket so that it can be reused
    memset(self, 0, sizeof(*self));
}

//----------------------------------------------------------------------
static GG_Result
GG_LwipDatagramSocket_Attach(GG_DatagramSocket* _self, GG_Loop* loop)
{
    GG_ASSERT(_self);
    GG_LwipDatagramSocket* self = GG_SELF(GG_LwipDatagramSocket, GG_DatagramSocket);
    GG_THREAD_GUARD_CHECK_BINDING(self);

    // check that we're not already attached
    if (self->loop) {
        return GG_ERROR_INVALID_STATE;
    }

    // keep a reference to the loop
    self->loop = loop;

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
static void
GG_LwipDatagramSocket_OnDataReceived(void* args)
{
    GG_LwipDatagramSocket* self = args;

    GG_LOG_FINEST("data received");

    // retrieve all pending buffers
    for (;;) {
        struct netbuf* buffer = NULL;
        int lwip_result = netconn_recv(self->udp_socket, &buffer);
        if (lwip_result != ERR_OK || !buffer) {
            GG_LOG_FINEST("processed all buffers (final code=%d)", lwip_result);
            break;
        }

        // check that we have a sink, drop the packet if we don't
        if (self->data_sink == NULL) {
            GG_LOG_FINE("no sink, dropping the packet");
            netbuf_free(buffer);
            continue;
        }

        // setup the metadata
        GG_SocketAddressMetadata metadata = GG_SOURCE_SOCKET_ADDRESS_METADATA_INITIALIZER(
            GG_IP_ADDRESS_NULL_INITIALIZER, netbuf_fromport(buffer));
        const ip_addr_t* source_address = netbuf_fromaddr(buffer);
        LwipIpAddressToIpAddress(source_address, &metadata.socket_address.address);

        // convert the data to a gg buffer
        GG_LOG_FINER("received packet with %u bytes", (unsigned int)netbuf_len(buffer));
        GG_DynamicBuffer* data = NULL;
        GG_Result result = GG_DynamicBuffer_Create(netbuf_len(buffer), &data);
        if (GG_FAILED(result)) {
            GG_LOG_WARNING("failed to allocate buffer (%d)", result);

            netbuf_free(buffer);
            continue;
        }
        GG_DynamicBuffer_SetDataSize(data, netbuf_len(buffer));
        netbuf_copy(buffer, GG_DynamicBuffer_UseData(data), netbuf_len(buffer));
        netbuf_free(buffer);

        // try to send
        // NOTE: there's nothing we can really do here if this fails, as this implementation doesn't
        // maintain a buffer queue
        GG_DataSink_PutData(self->data_sink, GG_DynamicBuffer_AsBuffer(data), &metadata.base);
        GG_DynamicBuffer_Release(data);
    }
}

//----------------------------------------------------------------------
static void
GG_LwipDatagramSocket_OnEvent(struct netconn* udp_socket, enum netconn_evt event_type, u16_t length)
{
    GG_COMPILER_UNUSED(length);

    if (event_type == NETCONN_EVT_RCVPLUS) {
        // look for a matching socket
        GG_LwipDatagramSocket* socket = NULL;
        for (size_t i = 0; i < GG_ARRAY_SIZE(GG_LwipSockets); i++) {
            socket = &GG_LwipSockets[i];
            if (socket->udp_socket == udp_socket) {
                break;
            }
        }
        if (!socket) {
            GG_LOG_SEVERE("callback for non-existant socket");
            return;
        }

        // invoke the callback on the sockets' loop thread
        GG_Loop_InvokeAsync(socket->loop, GG_LwipDatagramSocket_OnDataReceived, socket);
    }
}

//----------------------------------------------------------------------
static GG_Result
GG_LwipDatagramSocket_PutData(GG_DataSink*             _self,
                              GG_Buffer*               data,
                              const GG_BufferMetadata* metadata)
{
    GG_ASSERT(data);
    GG_COMPILER_UNUSED(metadata);
    GG_LwipDatagramSocket* self = GG_SELF(GG_LwipDatagramSocket, GG_DataSink);
    GG_THREAD_GUARD_CHECK_BINDING(self);

    // check that the buffer isn't too large
    if (GG_Buffer_GetDataSize(data) > self->max_datagram_size) {
        return GG_ERROR_INVALID_PARAMETERS;
    }

    // convert the data into an LWIP buffer
    u16_t buffer_size = (u16_t)GG_Buffer_GetDataSize(data);
    struct netbuf buffer = { 0 };
    void* payload = netbuf_alloc(&buffer, buffer_size);
    if (!payload) {
        return GG_ERROR_OUT_OF_MEMORY;
    }
    netbuf_take(&buffer, GG_Buffer_GetData(data), buffer_size);

    // decide where to send
    err_t result;
    if (self->connected) {
        // try to send the payload
        GG_LOG_FINEST("calling netconn_send");
        result = netconn_send(self->udp_socket, &buffer);
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
            netbuf_free(&buffer);
            return GG_ERROR_INVALID_STATE;
        }

        // try to send the payload
        GG_LOG_FINEST("calling netconn_sendto");
        result = netconn_sendto(self->udp_socket, &buffer, &destination_address, destination_port);
        GG_LOG_FINER("udp_sendto returned %d", (int)result);
    }

    netbuf_free(&buffer);

    if (result != ERR_OK) {
        if (result == ERR_MEM || result == ERR_WOULDBLOCK) {
            // the packet wasn't accepted by the network interface

            // get the timer scheduler for the socket's loop
            GG_TimerScheduler* timer_scheduler = GG_Loop_GetTimerScheduler(self->loop);
            if (timer_scheduler == NULL) {
                return GG_ERROR_INVALID_STATE;
            }

            // first, create a timer if we don't already have one.
            // (since this is a rare condition, we don't create the timer upfront)
            if (self->resend_timer == NULL) {
                // create the timer
                GG_Result gg_result = GG_TimerScheduler_CreateTimer(timer_scheduler, &self->resend_timer);
                if (GG_FAILED(gg_result)) {
                    return gg_result;
                }
            }

            // schedule to be called back in a short while
            GG_LOG_FINEST("scheduling UDP resend timer for %d ms", GG_LWIP_SOCKETS_RESEND_SLEEP_TIME_MS);
            GG_Result gg_result = GG_Timer_Schedule(self->resend_timer,
                                                    GG_CAST(self, GG_TimerListener),
                                                    GG_LWIP_SOCKETS_RESEND_SLEEP_TIME_MS);
            if (GG_SUCCEEDED(gg_result)) {
                // let the caller know they should try again later
                return GG_ERROR_WOULD_BLOCK;
            }
        }

        return MapErrorCode(result);
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
    // ignore this, as this implementation doesn't hold on to undelivered packets
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

    // notify our listener that they can try to put again
    GG_LOG_FINEST("resend timer fired");
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

    // find a place for the new socket
    GG_LwipDatagramSocket* self = NULL;
    for (size_t i = 0; i < GG_ARRAY_SIZE(GG_LwipSockets); i++) {
        if (GG_LwipSockets[i].udp_socket == NULL) {
            self = &GG_LwipSockets[i];
            memset(self, 0, sizeof(*self));
            break;
        }
    }
    if (self == NULL) {
        GG_LOG_WARNING("no more UDP sockets available");
        return GG_ERROR_OUT_OF_RESOURCES;
    }

    // create a UDP socket
    struct netconn* udp_socket = netconn_new_with_callback(NETCONN_UDP, GG_LwipDatagramSocket_OnEvent);
    if (udp_socket == NULL) {
        GG_LOG_SEVERE("netconn_new_with_callback failed");
        return GG_FAILURE;
    }

    // put the socket in non-blocking mode
    netconn_set_nonblocking(udp_socket, 1);

    // bind to the local address
    if (local_address) {
        ip_addr_t bind_address;
        IpAddressToLwipIpAddress(&local_address->address, &bind_address);
        err_t result = netconn_bind(udp_socket, &bind_address, local_address->port);
        if (result != ERR_OK) {
            GG_LOG_WARNING("netconn_bind failed (%d)", result);
            netconn_delete(udp_socket);
            return MapErrorCode(result);
        }
    }

    // connect to the remote address if specified
    if (remote_address && connect_to_remote) {
        ip_addr_t connect_address;
        IpAddressToLwipIpAddress(&remote_address->address, &connect_address);
        err_t result = netconn_connect(udp_socket, &connect_address, remote_address->port);
        if (result != ERR_OK) {
            GG_LOG_WARNING("netconn_connect failed (%d)", result);
            netconn_delete(udp_socket);
            return MapErrorCode(result);
        }
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

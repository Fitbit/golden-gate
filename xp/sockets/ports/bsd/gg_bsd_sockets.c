/**
 *
 * @file
 *
 * @copyright
 * Copyright 2017 by Fitbit, Inc., all rights reserved.
 *
 * @author Gilles Boccon-Gibod
 *
 * @date 2017-09-18
 *
 * @details
 *
 * BSD API implementation of the socket interfaces.
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "gg_bsd_sockets.h"
#include "xp/common/gg_utils.h"
#include "xp/common/gg_port.h"
#include "xp/common/gg_io.h"
#include "xp/common/gg_types.h"
#include "xp/common/gg_memory.h"
#include "xp/common/gg_timer.h"
#include "xp/common/gg_logging.h"
#include "xp/common/gg_threads.h"
#include "xp/sockets/gg_sockets.h"
#include "xp/loop/extensions/gg_loop_fd.h"
#include "xp/common/gg_port.h"

// Platform adaptation
#if GG_CONFIG_PLATFORM == GG_PLATFORM_BISON
// Bison platform with NetX
#include "fb_tx_vos.h"
#include "nx_bsd.h"

typedef INT       GG_SocketFd;
typedef INT       GG_socklen_t;
typedef in_addr_t GG_in_addr_t;
typedef ssize_t   GG_ssize_t;

#define INADDR_NONE (0xFFFFFFFF)
#define close(fd) soc_close(fd)

#elif GG_CONFIG_PLATFORM == GG_PLATFORM_WINDOWS
// Windows platform adaptation
#include <windows.h>

typedef SOCKET        GG_SocketFd;
typedef int           GG_socklen_t;
typedef unsigned long GG_in_addr_t;
typedef int           GG_ssize_t;

#define close(fd)                       closesocket(fd)
#define GetLastSocketError()            WSAGetLastError()
#define GG_BSD_SOCKET_IS_INVALID(_s)    ((_s) == INVALID_SOCKET)
#define GG_BSD_SOCKET_CALL_FAILED(_e)   ((_e) == SOCKET_ERROR)

#else
// standard unix/BSD

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <errno.h>

typedef int       GG_SocketFd;
typedef socklen_t GG_socklen_t;
typedef in_addr_t GG_in_addr_t;
typedef ssize_t   GG_ssize_t;

#endif

// defaults
#if !defined(GetLastSocketError)
#define GetLastSocketError()            (errno)
#endif
#if !defined(GG_BSD_SOCKET_IS_INVALID)
#define GG_BSD_SOCKET_IS_INVALID(_s)    ((_s)  < 0)
#endif
#if !defined(GG_BSD_SOCKET_CALL_FAILED)
#define GG_BSD_SOCKET_CALL_FAILED(_e)   ((_e)  < 0)
#endif

/*----------------------------------------------------------------------
|   logging
+---------------------------------------------------------------------*/
GG_SET_LOCAL_LOGGER("gg.xp.sockets.bsd")

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/
#define GG_BSD_SOCKETS_MAX_DATAGRAM_SIZE             65536
#define GG_BSD_DATAGRAM_SOCKET_MAX_RESEND_SLEEP_TIME 128   // milliseconds
#define GG_BSD_DATAGRAM_SOCKET_MIN_RESEND_SLEEP_TIME 8     // milliseconds

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
typedef union {
    struct sockaddr    sa;
    struct sockaddr_in sa_in;
} GG_sockaddr;

typedef struct {
    GG_IMPLEMENTS(GG_DatagramSocket);
    GG_IMPLEMENTS(GG_DataSink);
    GG_IMPLEMENTS(GG_DataSource);
    GG_IMPLEMENTS(GG_DataSinkListener);
    GG_IMPLEMENTS(GG_TimerListener);

    GG_LoopFileDescriptorEventHandler handler;
    GG_Loop*                          loop;
    GG_DataSink*                      data_sink;
    unsigned int                      max_datagram_size;
    GG_SocketAddress                  local_address;
    GG_SocketAddress                  remote_address;
    bool                              auto_bind;
    bool                              connected;
    GG_SocketFd                       fd;
    GG_DataSinkListener*              sink_listener;
    GG_Timer*                         resend_timer;
    uint32_t                          resend_sleep_time;

    GG_THREAD_GUARD_ENABLE_BINDING
} GG_BsdDatagramSocket;

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

//----------------------------------------------------------------------
static void
SocketAddressToInetAddress(const GG_SocketAddress* socket_address,
                           GG_sockaddr*            inet_address,
                           GG_socklen_t*           inet_address_length)
{
    GG_ASSERT(socket_address);
    GG_ASSERT(inet_address);
    GG_ASSERT(inet_address_length);

    // initialize the structure
    memset(inet_address, 0, sizeof(*inet_address));
    if (inet_address_length) {
        *inet_address_length = sizeof(struct sockaddr_in);
    }

#if defined(GG_CONFIG_HAVE_SOCKADDR_IN_SIN_LEN)
    inet_address->sa_in.sin_len = sizeof(inet_address);
#endif

    // setup the structure
    inet_address->sa_in.sin_family      = AF_INET;
    inet_address->sa_in.sin_port        = htons(socket_address->port);
    inet_address->sa_in.sin_addr.s_addr = htonl(GG_IpAddress_AsInteger(&socket_address->address));
}

//----------------------------------------------------------------------
static void
InetAddressToSocketAddress(const GG_sockaddr* inet_address,
                           GG_SocketAddress*  socket_address)
{
    GG_ASSERT(inet_address);
    GG_ASSERT(socket_address);

    socket_address->port = ntohs(inet_address->sa_in.sin_port);
    GG_IpAddress_SetFromInteger(&socket_address->address, ntohl(inet_address->sa_in.sin_addr.s_addr));
}

//----------------------------------------------------------------------
static GG_Result
MapErrorCode(int error)
{
    switch (error) {
        case ECONNRESET:
        case ENETRESET:
            return GG_ERROR_CONNECTION_RESET;

        case ECONNABORTED:
            return GG_ERROR_CONNECTION_ABORTED;

        case ECONNREFUSED:
            return GG_ERROR_CONNECTION_REFUSED;

        case ETIMEDOUT:
            return GG_ERROR_TIMEOUT;

        case EADDRINUSE:
            return GG_ERROR_ADDRESS_IN_USE;

        case ENETDOWN:
            return GG_ERROR_NETWORK_DOWN;

        case ENETUNREACH:
            return GG_ERROR_NETWORK_UNREACHABLE;

        case EHOSTUNREACH:
            return GG_ERROR_HOST_UNREACHABLE;

        case ENOBUFS:
            return GG_ERROR_OUT_OF_RESOURCES;

        case EINPROGRESS:
        case EAGAIN:
#if defined(EWOULDBLOCK) && (EWOULDBLOCK != EAGAIN)
        case EWOULDBLOCK:
#endif
            return GG_ERROR_WOULD_BLOCK;

#if defined(EPIPE)
        case EPIPE:
            return GG_ERROR_CONNECTION_RESET;
#endif

#if defined(ENOTCONN)
        case ENOTCONN:
            return GG_ERROR_NOT_CONNECTED;
#endif

#if defined(EINTR)
        case EINTR:
            return GG_ERROR_INTERRUPTED;
#endif

#if defined(EACCES)
        case EACCES:
            return GG_ERROR_PERMISSION_DENIED;
#endif

        default:
            return GG_ERROR_ERRNO(error);
    }
}

//----------------------------------------------------------------------
#if GG_CONFIG_PLATFORM == GG_PLATFORM_WINDOWS
static GG_Result
SetNonBlocking(GG_SocketFd fd)
{
    u_long args = 1;
    if (ioctlsocket(fd, FIONBIO, &args)) {
        return GG_ERROR_SOCKET_CONTROL_FAILED;
    }
    return GG_SUCCESS;
}
#else
static GG_Result
SetNonBlocking(GG_SocketFd fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    flags |= O_NONBLOCK;
    if (fcntl(fd, F_SETFL, flags)) {
        return MapErrorCode(GetLastSocketError());
    }
    return GG_SUCCESS;
}
#endif

//----------------------------------------------------------------------
static GG_DataSink*
GG_BsdDatagramSocket_AsDataSink(GG_DatagramSocket* _self)
{
    GG_ASSERT(_self);
    GG_BsdDatagramSocket* self = GG_SELF(GG_BsdDatagramSocket, GG_DatagramSocket);

    return GG_CAST(self, GG_DataSink);
}

//----------------------------------------------------------------------
static GG_DataSource*
GG_BsdDatagramSocket_AsDataSource(GG_DatagramSocket* _self)
{
    GG_ASSERT(_self);
    GG_BsdDatagramSocket* self = GG_SELF(GG_BsdDatagramSocket, GG_DatagramSocket);

    return GG_CAST(self, GG_DataSource);
}

//----------------------------------------------------------------------
static void
GG_BsdDatagramSocket_Destroy(GG_DatagramSocket* _self)
{
    if (!_self) return;
    GG_BsdDatagramSocket* self = GG_SELF(GG_BsdDatagramSocket, GG_DatagramSocket);
    GG_THREAD_GUARD_CHECK_BINDING(self);

    // destroy the resend timer if we have one
    GG_Timer_Destroy(self->resend_timer);

    // de-register from the loop
    if (self->loop) {
        GG_Loop_RemoveFileDescriptorHandler(self->loop, &self->handler);
    }

    // close the socket
    close(self->fd);

    // free the memory
    GG_ClearAndFreeObject(self, 5);
}

//----------------------------------------------------------------------
static GG_Result
GG_BsdDatagramSocket_Attach(GG_DatagramSocket* _self, GG_Loop* loop)
{
    GG_ASSERT(_self);
    GG_BsdDatagramSocket* self = GG_SELF(GG_BsdDatagramSocket, GG_DatagramSocket);
    GG_THREAD_GUARD_CHECK_BINDING(self);

    // check that we're not already attached
    if (self->loop) {
        return GG_ERROR_INVALID_STATE;
    }

    // we're now attached to that loop
    self->loop = loop;

    // register as a handler with the loop
    GG_LOG_FINE("registering handler");
    GG_Result result = GG_Loop_AddFileDescriptorHandler(loop, &self->handler);
    if (GG_FAILED(result)) {
        return result;
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
static GG_Result
GG_BsdDatagramSocket_TryToSend(GG_BsdDatagramSocket*    self,
                               GG_Buffer*               data,
                               const GG_BufferMetadata* metadata)
{
    // decide where to send
    GG_ssize_t io_result;
    if (self->connected) {
        // try to send the payload
        do {
            io_result = send(self->fd,
                             (const void*)GG_Buffer_GetData(data),
                             (size_t)GG_Buffer_GetDataSize(data),
                             0);
            GG_LOG_FINER("send returned %d", (int)io_result);
        } while (GG_BSD_SOCKET_CALL_FAILED(io_result) && GetLastSocketError() == EINTR);
    } else {
        GG_sockaddr  destination_address;
        GG_socklen_t destination_address_length;
        if (metadata && metadata->type == GG_BUFFER_METADATA_TYPE_DESTINATION_SOCKET_ADDRESS) {
            SocketAddressToInetAddress(&((const GG_SocketAddressMetadata*)metadata)->socket_address,
                                       &destination_address,
                                       &destination_address_length);
        } else if (self->remote_address.port) {
            SocketAddressToInetAddress(&self->remote_address,
                                       &destination_address,
                                       &destination_address_length);
        } else {
            return GG_ERROR_INVALID_STATE;
        }

        // try to send the payload
        do {
            io_result = sendto(self->fd,
                               (const void*)GG_Buffer_GetData(data),
                               (size_t)GG_Buffer_GetDataSize(data),
                               0,
                               &destination_address.sa,
                               destination_address_length);
            GG_LOG_FINER("sendto returned %d", (int)io_result);
        } while (GG_BSD_SOCKET_CALL_FAILED(io_result) && GetLastSocketError() == EINTR);
    }

    if (GG_BSD_SOCKET_CALL_FAILED(io_result)) {
        GG_LOG_FINER("sendto error = %d", (int)GetLastSocketError());
        return MapErrorCode(GetLastSocketError());
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
static GG_Result
GG_BsdDatagramSocket_PutData(GG_DataSink*             _self,
                             GG_Buffer*               data,
                             const GG_BufferMetadata* metadata)
{
    GG_ASSERT(_self);
    GG_ASSERT(data);
    GG_BsdDatagramSocket* self = GG_SELF(GG_BsdDatagramSocket, GG_DataSink);
    GG_THREAD_GUARD_CHECK_BINDING(self);

    // try to send
    GG_Result result = GG_BsdDatagramSocket_TryToSend(self, data, metadata);
    if (GG_SUCCEEDED(result)) {
        // cancel any active resend timer and reset the exponential back-off counter
        self->resend_sleep_time = 0;
        if (self->resend_timer) {
            GG_LOG_FINEST("packet sent, canceling resend timer");
            GG_Timer_Unschedule(self->resend_timer);
        }
    } else if (result == GG_ERROR_WOULD_BLOCK) {
        // let the monitor know we'd like to be called when we CAN_WRITE
        self->handler.event_mask |= GG_EVENT_FLAG_FD_CAN_WRITE;
    } else if (result == GG_ERROR_OUT_OF_RESOURCES) {
        // On some systems, the only UDP flow control mechanism is to return
        // ENOBUFS when the outgoing interface buffer is full. There's no way
        // to use select() to wait until space becomes available, so we need
        // to set a timer to retry.

        // first, create a timer if we don't already have one.
        // (since this is a rare condition, we don't create the timer upfront)
        if (self->resend_timer == NULL) {
            result = GG_TimerScheduler_CreateTimer(GG_Loop_GetTimerScheduler(self->loop), &self->resend_timer);
            if (GG_FAILED(result)) return result;
        }

        // schedule to be called back in a short while.
        self->resend_sleep_time = GG_MAX(self->resend_sleep_time, GG_BSD_DATAGRAM_SOCKET_MIN_RESEND_SLEEP_TIME);
        GG_LOG_FINEST("scheduling UDP resend timer for %u ms", (int)self->resend_sleep_time);
        result = GG_Timer_Schedule(self->resend_timer,
                                   GG_CAST(self, GG_TimerListener),
                                   self->resend_sleep_time);
        if (GG_SUCCEEDED(result)) {
            // let the caller know they should try again later
            result = GG_ERROR_WOULD_BLOCK;
        }
    }

    return result;
}

//----------------------------------------------------------------------
static GG_Result
GG_BsdDatagramSocket_SetListener(GG_DataSink* _self, GG_DataSinkListener* listener)
{
    GG_ASSERT(_self);
    GG_BsdDatagramSocket* self = GG_SELF(GG_BsdDatagramSocket, GG_DataSink);
    GG_THREAD_GUARD_CHECK_BINDING(self);

    self->sink_listener = listener;

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
static GG_Result
GG_BsdDatagramSocket_SetDataSink(GG_DataSource* _self, GG_DataSink* data_sink)
{
    GG_ASSERT(_self);
    GG_BsdDatagramSocket* self = GG_SELF(GG_BsdDatagramSocket, GG_DataSource);
    GG_THREAD_GUARD_CHECK_BINDING(self);

    // keep a reference to the sink
    self->data_sink = data_sink;

    // express an interest in being notified when data is available to read
    self->handler.event_mask |= GG_EVENT_FLAG_FD_CAN_READ;

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
static void
GG_BsdDatagramSocket_OnEvent(GG_LoopEventHandler* _self, GG_Loop* loop)
{
    GG_ASSERT(_self);
    GG_COMPILER_UNUSED(loop);
    GG_BsdDatagramSocket* self = GG_SELF_M(handler.base, GG_BsdDatagramSocket, GG_LoopEventHandler);
    GG_THREAD_GUARD_CHECK_BINDING(self);

    GG_ASSERT(self->fd == self->handler.fd);
    GG_LOG_FINER("got event for FD %d, flags=%d", self->fd, self->handler.event_flags);

    // check if we can read
    if (self->handler.event_flags & GG_EVENT_FLAG_FD_CAN_READ) {
        GG_ASSERT(self->data_sink);

        // allocate a buffer to read into
        // TEMP: use a dynamic buffer fow now, but we should get a buffer from a pool
        GG_DynamicBuffer* buffer = NULL;
        GG_Result result = GG_DynamicBuffer_Create(self->max_datagram_size, &buffer);
        if (GG_SUCCEEDED(result)) {
            // read a datagram
            GG_sockaddr  sender_address;
            GG_socklen_t sender_address_length = sizeof(sender_address);
            GG_ssize_t io_result;
            do {
                io_result = recvfrom(self->fd,
                                     (void*)GG_DynamicBuffer_UseData(buffer),
                                     (size_t)self->max_datagram_size,
                                     0,
                                     &sender_address.sa,
                                     &sender_address_length);
                GG_LOG_FINER("recvfrom returned %d", (int)io_result);
            } while (GG_BSD_SOCKET_CALL_FAILED(io_result) && GetLastSocketError() == EINTR);
            if (io_result >= 0) {
                // we now know how much data was received
                GG_DynamicBuffer_SetDataSize(buffer, (size_t)io_result);

                // setup the metadata
                GG_SocketAddressMetadata metadata = GG_SOURCE_SOCKET_ADDRESS_METADATA_INITIALIZER(
                    GG_IP_ADDRESS_NULL_INITIALIZER, (uint16_t)sender_address.sa_in.sin_port);
                InetAddressToSocketAddress(&sender_address, &metadata.socket_address);

                // if in auto-bind mode, save remote address to be used to send back data
                if (self->auto_bind) {
                    InetAddressToSocketAddress(&sender_address, &self->remote_address);
#if defined(GG_CONFIG_ENABLE_LOGGING)
                    char address_str[20];
                    GG_SocketAddress_AsString(&self->remote_address, address_str, sizeof(address_str));
                    GG_LOG_FINER("auto-binding to %s", address_str);
#endif
                }

                // push the data to the sink (ignore errors for now)
                GG_DataSink_PutData(self->data_sink, GG_DynamicBuffer_AsBuffer(buffer), &metadata.base);

                // we don't need this buffer anymore
                GG_DynamicBuffer_Release(buffer);
            }
        } else {
            GG_LOG_SEVERE("failed to allocate read buffer (%d)", result);

            // don't read anymore to avoid looping forever
            self->handler.event_mask &= ~GG_EVENT_FLAG_FD_CAN_READ;
        }
    }

    // check if we can write
    if (self->handler.event_flags & GG_EVENT_FLAG_FD_CAN_WRITE) {
        // notify our listener that they can try to put again
        if (self->sink_listener) {
            GG_DataSinkListener_OnCanPut(self->sink_listener);
        }

        // we don't need to monitor CAN_WRITE anymore
        self->handler.event_mask &= ~GG_EVENT_FLAG_FD_CAN_WRITE;
    }
}

//----------------------------------------------------------------------
static void
GG_BsdDatagramSocket_OnTimerFired(GG_TimerListener* _self, GG_Timer* timer, uint32_t elapsed)
{
    GG_COMPILER_UNUSED(timer);
    GG_COMPILER_UNUSED(elapsed);
    GG_BsdDatagramSocket* self = GG_SELF(GG_BsdDatagramSocket, GG_TimerListener);
    GG_THREAD_GUARD_CHECK_BINDING(self);

    GG_LOG_FINEST("resend timer fired");

    // adjust the timer based on a capped exponential back-off
    self->resend_sleep_time = GG_MIN(2 * self->resend_sleep_time, GG_BSD_DATAGRAM_SOCKET_MAX_RESEND_SLEEP_TIME);

    // notify our listener that they can try to put again
    if (self->sink_listener) {
        GG_DataSinkListener_OnCanPut(self->sink_listener);
    }
}

/*----------------------------------------------------------------------
|   function table
+---------------------------------------------------------------------*/
GG_IMPLEMENT_INTERFACE(GG_BsdDatagramSocket, GG_DatagramSocket) {
    GG_BsdDatagramSocket_AsDataSink,
    GG_BsdDatagramSocket_AsDataSource,
    GG_BsdDatagramSocket_Destroy,
    GG_BsdDatagramSocket_Attach
};

GG_IMPLEMENT_INTERFACE(GG_BsdDatagramSocket, GG_DataSink) {
    GG_BsdDatagramSocket_PutData,
    GG_BsdDatagramSocket_SetListener
};

GG_IMPLEMENT_INTERFACE(GG_BsdDatagramSocket, GG_DataSource) {
    GG_BsdDatagramSocket_SetDataSink
};

GG_IMPLEMENT_INTERFACE(GG_BsdDatagramSocket, GG_LoopEventHandler) {
    GG_BsdDatagramSocket_OnEvent
};

GG_IMPLEMENT_INTERFACE(GG_BsdDatagramSocket, GG_TimerListener) {
    GG_BsdDatagramSocket_OnTimerFired
};

//----------------------------------------------------------------------
GG_Result
GG_BsdDatagramSocket_Create(const GG_SocketAddress* local_address,
                            const GG_SocketAddress* remote_address,
                            bool                    connect_to_remote,
                            unsigned int            max_datagram_size,
                            GG_DatagramSocket**     socket_object)
{
    // default return value
    *socket_object = NULL;

    // check parameters
    if (max_datagram_size > GG_BSD_SOCKETS_MAX_DATAGRAM_SIZE) {
        return GG_ERROR_INVALID_PARAMETERS;
    }

    // create a UDP socket
    GG_SocketFd fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (GG_BSD_SOCKET_IS_INVALID(fd)) {
        GG_LOG_WARNING("socket() failed (%d)", (int)GetLastSocketError());
        return MapErrorCode(GetLastSocketError());
    }

    // put the socket in non-blocking mode
    GG_Result result = SetNonBlocking(fd);
    if (GG_FAILED(result)) {
        close(fd);
        return result;
    }

    // set socket options (ignore errors)
    int option = 1;
    setsockopt(fd,
               SOL_SOCKET,
               SO_REUSEADDR,
               (void*)&option,
               sizeof(option));

    // bind to the local address
    if (local_address) {
        GG_sockaddr  bind_address;
        GG_socklen_t bind_address_length;
        SocketAddressToInetAddress(local_address, &bind_address, &bind_address_length);
        int bsd_result = bind(fd, &bind_address.sa, bind_address_length);
        if (bsd_result != 0) {
            close(fd);
            return MapErrorCode(GetLastSocketError());
        }
    }

    // connect to the remote address if specified
    if (remote_address && connect_to_remote) {
        GG_sockaddr  connect_address;
        GG_socklen_t connect_address_length;
        SocketAddressToInetAddress(remote_address, &connect_address, &connect_address_length);
        int bsd_result = connect(fd, &connect_address.sa, connect_address_length);
        if (GG_BSD_SOCKET_CALL_FAILED(bsd_result)) {
            close(fd);
            return MapErrorCode(GetLastSocketError());
        }
    }

    // allocate a new object
    GG_BsdDatagramSocket* self = (GG_BsdDatagramSocket*)GG_AllocateZeroMemory(sizeof(GG_BsdDatagramSocket));
    if (self == NULL) {
        close(fd);
        return GG_ERROR_OUT_OF_MEMORY;
    }

    // init the instance
    self->fd                = fd;
    self->max_datagram_size = max_datagram_size;
    if (local_address) {
        self->local_address = *local_address;
    }
    if (remote_address) {
        self->remote_address = *remote_address;
    } else {
        self->auto_bind = true;
    }
    self->connected = connect_to_remote;

    // init the event handler base
    self->handler.fd          = fd;
    self->handler.event_flags = 0;
    self->handler.event_mask  = GG_EVENT_FLAG_FD_ERROR;
    GG_SET_INTERFACE(&self->handler.base, GG_BsdDatagramSocket, GG_LoopEventHandler);

    // init the function tables
    GG_SET_INTERFACE(self, GG_BsdDatagramSocket, GG_DatagramSocket);
    GG_SET_INTERFACE(self, GG_BsdDatagramSocket, GG_DataSink);
    GG_SET_INTERFACE(self, GG_BsdDatagramSocket, GG_DataSource);
    GG_SET_INTERFACE(self, GG_BsdDatagramSocket, GG_TimerListener);

    // bind to the current thread
    GG_THREAD_GUARD_BIND(self);

    // return the object
    *socket_object = GG_CAST(self, GG_DatagramSocket);

    return GG_SUCCESS;
}


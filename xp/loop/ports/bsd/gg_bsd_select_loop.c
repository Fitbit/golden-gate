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
 * @date 2017-09-25
 *
 * @details
 *
 * Event loop implementation based on the BSD select() API.
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include "xp/common/gg_types.h"
#include "xp/common/gg_utils.h"
#include "xp/common/gg_lists.h"
#include "xp/common/gg_queues.h"
#include "xp/common/gg_memory.h"
#include "xp/common/gg_system.h"
#include "xp/common/gg_logging.h"
#include "xp/common/gg_results.h"
#include "xp/common/gg_port.h"
#include "xp/common/gg_threads.h"
#include "xp/loop/gg_loop.h"
#include "xp/loop/gg_loop_base.h"
#include "xp/loop/extensions/gg_loop_fd.h"

// Platform adaptation
#if GG_CONFIG_PLATFORM == GG_PLATFORM_BISON
// Bison platform with NetX
#include "fb_tx_vos.h"
#include "nx_bsd.h"

typedef INT     GG_SocketFd;
typedef INT     GG_socklen_t;
typedef ssize_t GG_ssize_t;

#define close(fd) soc_close(fd)

// on Bison, select() is broken and can't wait for timeouts less than 1 cpu tick
#define GG_BSD_SELECT_MIN_SLEEP_TIME_MICROSECONDS  (NX_MICROSECOND_PER_CPU_TICK)

#elif GG_CONFIG_PLATFORM == GG_PLATFORM_WINDOWS
// Windows platform
#include <windows.h>

#define GG_CONFIG_ENABLE_BSD_SOCKETPAIR_EMULATION

typedef SOCKET GG_SocketFd;
typedef int    GG_socklen_t;
typedef int    GG_ssize_t;

#define close(fd)                       closesocket(fd)
#define GetLastSocketError()            WSAGetLastError()
#define GG_BSD_SOCKET_CALL_FAILED(_e)   ((_e) == SOCKET_ERROR)
#define GG_BSD_SOCKET_INVALID_HANDLE    INVALID_SOCKET
#define GG_BSD_SOCKET_IS_INVALID(_s)    ((_s) == INVALID_SOCKET)

#else
// standard unix/BSD
#include <string.h> // include this first, as some platforms reference memset in the FD_XXX macros
#include <sys/select.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

typedef int       GG_SocketFd;
typedef socklen_t GG_socklen_t;
typedef ssize_t   GG_ssize_t;

#endif

#if defined(GG_CONFIG_ENABLE_BSD_SOCKETPAIR_EMULATION)
#if GG_CONFIG_PLATFORM == GG_PLATFORM_BISON
#define INADDR_LOOPBACK 0x7f000001
#else
#include <netinet/in.h>
#include <arpa/inet.h>
#endif
#endif

#if defined(GG_CONFIG_ENABLE_PER_PID_SOCKETPAIR_FD)
#if !defined(GG_INVALID_PID_VALUE)
#define GG_INVALID_PID_VALUE 0
#endif
#define GG_BSD_SELECT_LOOP_MAX_PIDS 8  // maximum number of tasks that can have their
                                       // own FD to post to the wakeup socket
#if !defined(GG_CONFIG_ENABLE_BSD_SOCKETPAIR_EMULATION)
// sanity check
#error GG_CONFIG_ENABLE_PER_PID_SOCKETPAIR_FD requires GG_CONFIG_ENABLE_BSD_SOCKETPAIR_EMULATION
#endif
#endif

// defaults
#if !defined(GetLastSocketError)
#define GetLastSocketError()            (errno)
#endif
#if !defined(GG_BSD_SOCKET_CALL_FAILED)
#define GG_BSD_SOCKET_CALL_FAILED(_e)   ((_e)  < 0)
#endif
#if !defined(GG_BSD_SOCKET_INVALID_HANDLE)
#define GG_BSD_SOCKET_INVALID_HANDLE    (-1)
#endif
#if !defined(GG_BSD_SOCKET_IS_INVALID)
#define GG_BSD_SOCKET_IS_INVALID(_s)    ((_s)  < 0)
#endif

/*----------------------------------------------------------------------
|   logging
+---------------------------------------------------------------------*/
GG_SET_LOCAL_LOGGER("gg.xp.loop.bsd-select")

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
struct GG_Loop {
    // inherited base class
    GG_LoopBase base;

    // subclass members
    GG_LoopFileDescriptorEventHandler wakeup_handler;
    GG_SocketFd                       wakeup_read_fd;
# if defined(GG_CONFIG_ENABLE_PER_PID_SOCKETPAIR_FD)
    struct {
        pid_t       pid;
        GG_SocketFd fd;
    }                                 wakeup_write_fds[GG_BSD_SELECT_LOOP_MAX_PIDS];
    GG_Mutex*                         wakeup_write_fds_lock;
#else
    GG_SocketFd                       wakeup_write_fd;
#endif
#if defined(GG_CONFIG_ENABLE_BSD_SOCKETPAIR_EMULATION)
    struct sockaddr_in                wakeup_read_socket_address;
#endif
    GG_LinkedList                     monitor_handlers;
};

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

//----------------------------------------------------------------------
#if defined(GG_CONFIG_ENABLE_INSPECTION)
GG_Inspectable*
GG_Loop_AsInspectable(GG_Loop* self)
{
    return GG_CAST(&self->base, GG_Inspectable);
}

static GG_Result
GG_Loop_Inspect(GG_Inspectable* _self, GG_Inspector* inspector, const GG_InspectionOptions* options)
{
    GG_COMPILER_UNUSED(options);
    GG_Loop* self = GG_SELF_M(base, GG_Loop, GG_Inspectable);

    GG_Inspector_OnInteger(inspector, "start_time", (int64_t)self->base.start_time, GG_INSPECTOR_FORMAT_HINT_UNSIGNED);
    GG_Inspector_OnArrayStart(inspector, "monitors");
    GG_LINKED_LIST_FOREACH(node, &self->monitor_handlers) {
        GG_LoopFileDescriptorEventHandler* handler =
            GG_LINKED_LIST_ITEM(node, GG_LoopFileDescriptorEventHandler, base.list_node);
        GG_Inspector_OnObjectStart(inspector, NULL);
        GG_Inspector_OnInteger(inspector, "fd",          handler->fd,          GG_INSPECTOR_FORMAT_HINT_NONE);
        GG_Inspector_OnInteger(inspector, "event_flags", handler->event_flags, GG_INSPECTOR_FORMAT_HINT_HEX);
        GG_Inspector_OnInteger(inspector, "event_mask",  handler->event_mask,  GG_INSPECTOR_FORMAT_HINT_HEX);
        GG_Inspector_OnObjectEnd(inspector);
    }
    GG_Inspector_OnArrayEnd(inspector);

    return GG_SUCCESS;
}

GG_IMPLEMENT_INTERFACE(GG_Loop, GG_Inspectable) {
    .Inspect = GG_Loop_Inspect
};
#endif

//----------------------------------------------------------------------
static GG_Result
GG_Loop_MonitorFileDescriptors(GG_Loop* self, uint32_t max_wait_time_ms)
{
    fd_set read_set;
    fd_set write_set;
    fd_set except_set;

    FD_ZERO(&read_set);
    FD_ZERO(&write_set);
    FD_ZERO(&except_set);

    // setup the sets
    int max_fd = -1;
    GG_LINKED_LIST_FOREACH(node, &self->monitor_handlers) {
        GG_LoopFileDescriptorEventHandler* handler =
            GG_LINKED_LIST_ITEM(node, GG_LoopFileDescriptorEventHandler, base.list_node);

        // register for the conditions we want to monitor
        if (handler->fd >= 0) {
            if (handler->event_mask & GG_EVENT_FLAG_FD_CAN_READ) {
                FD_SET(handler->fd, &read_set);
            }
            if (handler->event_mask & GG_EVENT_FLAG_FD_CAN_WRITE) {
                FD_SET(handler->fd, &write_set);
            }
            if (handler->event_mask & GG_EVENT_FLAG_FD_ERROR) {
                FD_SET(handler->fd, &except_set);
            }

            if (handler->fd > max_fd) {
                max_fd = handler->fd;
            }
        }
    }

    // setup a timeout if needed
    struct timeval timeout = {0, 0};
    if (max_wait_time_ms != GG_TIMER_NEVER) {
        timeout.tv_sec  = max_wait_time_ms/GG_MILLISECONDS_PER_SECOND;
        timeout.tv_usec = 1000*(max_wait_time_ms%GG_MILLISECONDS_PER_SECOND);
#if defined(GG_BSD_SELECT_MIN_SLEEP_TIME_MICROSECONDS)
        if (timeout.tv_sec == 0 && timeout.tv_usec < GG_BSD_SELECT_MIN_SLEEP_TIME_MICROSECONDS) {
            timeout.tv_usec = GG_BSD_SELECT_MIN_SLEEP_TIME_MICROSECONDS;
        }
#endif
    }

    // wait for a file descriptor to be ready or a timeout
    int io_result;
    do {
        GG_LOG_FINER("waiting for events, timeout=%d", max_wait_time_ms);
        io_result = select(max_fd+1,
                           &read_set, &write_set, &except_set,
                           max_wait_time_ms == GG_TIMER_NEVER ? NULL : &timeout);
        GG_LOG_FINER("select returned %d", io_result);
    } while (GG_BSD_SOCKET_CALL_FAILED(io_result) && GetLastSocketError() == EINTR);

    // check for errors
    if (GG_BSD_SOCKET_CALL_FAILED(io_result)) {
        return GG_ERROR_ERRNO(GetLastSocketError());
    }

    // update the timer scheduler now so that its notion of time is current
    GG_LoopBase_UpdateTime(&self->base);

    // check which events have occurred
    if (io_result > 0) {
        // IMPORTANT NOTE
        // because we are calling handlers during an iteration of the handler
        // list, and handlers may be causing entries in the list to be
        // removed (ex: a socket being destroyed), we proceed in two steps:
        // * first, we move the list to a temporary list variable that we use
        //   as a queue of items to process
        // * then we process items in the queue, one by one, always taking the
        //   first item in the queue, processing it and then moving it back to
        //   the original list.
        // (since we're only looking at the head of the queue, and not
        // iterating further through it, that is safe)
        GG_LinkedList handler_queue = GG_LINKED_LIST_INITIALIZER(handler_queue);
        GG_LINKED_LIST_MOVE(&self->monitor_handlers, &handler_queue);
        while (!GG_LINKED_LIST_IS_EMPTY(&handler_queue)) {
            // pop the head of the handler queue
            GG_LinkedListNode* head;
            GG_LINKED_LIST_POP_HEAD(head, &handler_queue);

            // move it back to the original list
            GG_LINKED_LIST_APPEND(&self->monitor_handlers, head);

            // process the handler
            GG_LoopFileDescriptorEventHandler* handler =
                GG_LINKED_LIST_ITEM(head, GG_LoopFileDescriptorEventHandler, base.list_node);

            // shortcut
            if (handler->event_mask == 0) continue;

            // check which conditions are true
            uint32_t event_flags = 0;
            if (FD_ISSET(handler->fd, &read_set)) {
                event_flags |= GG_EVENT_FLAG_FD_CAN_READ;
            }
            if (FD_ISSET(handler->fd, &write_set)) {
                event_flags |= GG_EVENT_FLAG_FD_CAN_WRITE;
            }
            if (FD_ISSET(handler->fd, &except_set)) {
                event_flags |= GG_EVENT_FLAG_FD_ERROR;
            }

            // notify if there's a match
            // (there should be a match, but we check just in case the select()
            // API doesn't always behave as we expect).
            if (event_flags) {
                // store the flags
                handler->event_flags = event_flags;

                // call the listener
                GG_LoopEventHandler_OnEvent(GG_CAST(&handler->base, GG_LoopEventHandler), self);
            }
        }
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
// Inner loop, called from within an auto-release wrapper
// (because some platforms need the wrapper to avoid retainining released
// objects forever)
//
// Returns GG_SUCCESS if the loop should continue, or GG_FAILURE if it should stop
//----------------------------------------------------------------------
static GG_Result
GG_Loop_Inner(void* _self)
{
    GG_Loop* self = _self;

    // process all timers
    uint32_t max_wait_time = GG_LoopBase_CheckTimers(&self->base);

    // check for termination in case a timer handler requested it
    if (self->base.termination_requested) {
        return GG_ERROR_INTERRUPTED;
    }

    // wait for I/O or messages
    GG_Result result = GG_Loop_MonitorFileDescriptors(self, max_wait_time);
    if (GG_FAILED(result)) {
        GG_LOG_WARNING("GG_Loop_MonitorFileDescriptors failed (%d)", result);
        return result;
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_Result
GG_Loop_Run(GG_Loop* self)
{
    GG_LOG_INFO("loop starting");

    // if it isn't already bound, bind the loop to the current thread
    GG_ASSERT(!GG_THREAD_GUARD_IS_OBJECT_BOUND(&self->base) || GG_THREAD_GUARD_IS_CURRENT_THREAD_BOUND(&self->base));
    if (!GG_THREAD_GUARD_IS_OBJECT_BOUND(&self->base)) {
        GG_Result result = GG_Loop_BindToCurrentThread(self);
        if (GG_FAILED(result)) {
            return result;
        }
    }

    // loop until termination
    GG_Result result = GG_SUCCESS;
    self->base.termination_requested = false;
    while (!self->base.termination_requested) {
        // call the inner part of the loop from within a wrapper function
        result = GG_AutoreleaseWrap(GG_Loop_Inner, self);
        if (GG_FAILED(result)) {
            if (result == GG_ERROR_INTERRUPTED && self->base.termination_requested) {
                // that's a normal termination, don't report an error
                result = GG_SUCCESS;
            }
            break;
        }
    }
    GG_LOG_INFO("loop terminating");

    return result;
}


//----------------------------------------------------------------------
#if defined(GG_CONFIG_ENABLE_BSD_SOCKETPAIR_EMULATION)
static GG_Result
GG_Loop_CreateLoopbackSocket(GG_SocketFd* fd)
{
    // create the socket
    *fd = socket(PF_INET, SOCK_DGRAM, 0);
    if (GG_BSD_SOCKET_IS_INVALID(*fd)) {
        GG_Result result = GG_ERROR_ERRNO(GetLastSocketError());
        GG_LOG_WARNING("socket() failed (%d)", result);
        return result;
    }

    // bind the socket to the loopback interface
    struct sockaddr_in inet_address;
    memset(&inet_address, 0, sizeof(inet_address));
    inet_address.sin_family = AF_INET;
    inet_address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    int bind_result = bind(*fd, (const struct sockaddr*)&inet_address, sizeof(inet_address));
    if (bind_result != 0) {
        close(*fd);
        *fd = GG_BSD_SOCKET_INVALID_HANDLE;
        GG_Result result = GG_ERROR_ERRNO(GetLastSocketError());
        GG_LOG_WARNING("bind() failed (%d)", result);
        return result;
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
static GG_Result
GG_Loop_ConnectLoopbackSocket(GG_Loop* self, GG_SocketFd fd)
{
    int bsd_result = connect(fd,
                             (const struct sockaddr*)&self->wakeup_read_socket_address,
                             sizeof(self->wakeup_read_socket_address));
    if (bsd_result != 0) {
        GG_Result result = GG_ERROR_ERRNO(GetLastSocketError());
        GG_LOG_WARNING("connect() failed (%d)", result);
        return result;
    }

    return GG_SUCCESS;
}
#endif

//----------------------------------------------------------------------
#if defined(GG_CONFIG_ENABLE_PER_PID_SOCKETPAIR_FD)
static GG_SocketFd
GG_Loop_GetWakeupWriteFd(GG_Loop* self)
{
    GG_Mutex_Lock(self->wakeup_write_fds_lock);

    // check if there's already a socket for the current PID
    pid_t pid = getpid();
    for (unsigned int i = 0; i < GG_ARRAY_SIZE(self->wakeup_write_fds); i++) {
        if (self->wakeup_write_fds[i].pid == pid) {
            // the PID for this entry matches ours
            GG_Mutex_Unlock(self->wakeup_write_fds_lock);
            return self->wakeup_write_fds[i].fd;
        }
    }

    // look for a free entry in the list and create a socket if space is found
    for (unsigned int i = 0; i < GG_ARRAY_SIZE(self->wakeup_write_fds); i++) {
        if (self->wakeup_write_fds[i].pid == GG_INVALID_PID_VALUE) {
            GG_ASSERT(GG_BSD_SOCKET_IS_INVALID(self->wakeup_write_fds[i].fd));

            GG_LOG_FINE("creating loopback socket for pid %d", (int)pid);
            GG_Result result = GG_Loop_CreateLoopbackSocket(&self->wakeup_write_fds[i].fd);
            if (GG_FAILED(result)) {
                GG_LOG_WARNING("failed to create socket (%d)", result);
                break;
            }
            GG_LOG_FINE("connecting loopback socket for pid %d", (int)pid);
            result = GG_Loop_ConnectLoopbackSocket(self, self->wakeup_write_fds[i].fd);
            if (GG_FAILED(result)) {
                GG_LOG_WARNING("failed to connect socket (%d)", result);
                close(self->wakeup_write_fds[i].fd);
                self->wakeup_write_fds[i].fd = GG_BSD_SOCKET_INVALID_HANDLE;
                break;
            }

            // remember the pid
            self->wakeup_write_fds[i].pid = pid;

            GG_Mutex_Unlock(self->wakeup_write_fds_lock);
            return self->wakeup_write_fds[i].fd;
        }
    }

    GG_Mutex_Unlock(self->wakeup_write_fds_lock);
    return GG_BSD_SOCKET_INVALID_HANDLE;
}
#else
static GG_SocketFd
GG_Loop_GetWakeupWriteFd(GG_Loop* self)
{
    return self->wakeup_write_fd;
}
#endif

//----------------------------------------------------------------------
static GG_Result
GG_Loop_SendWakeup(GG_Loop* self)
{
    uint8_t msg = 0;
    GG_ssize_t io_result;
    do {
        // get the file descriptor to write to
        GG_SocketFd wakeup_fd = GG_Loop_GetWakeupWriteFd(self);
        if (GG_BSD_SOCKET_IS_INVALID(wakeup_fd)) {
            GG_LOG_WARNING("failed to get wakeup FD");
            return GG_ERROR_OUT_OF_RESOURCES;
        }

        // write to the file descriptor to wake up the loop
        GG_LOG_FINEST("writing to wakeup fd");
        io_result = send(wakeup_fd, (void*)&msg, 1, 0);
    } while (GG_BSD_SOCKET_CALL_FAILED(io_result) && GetLastSocketError() == EINTR);

    if (GG_BSD_SOCKET_CALL_FAILED(io_result)) {
        // ignore non-fatal errors that indicate that the wakeup fd buffers
        // are full, because in that case we don't really need to write any
        // more data to it, since the other side will wake up anyway from
        // what's already pending.
        if (GetLastSocketError() != ENOBUFS) {
            GG_LOG_WARNING("send failed, errno=%d", (int)GetLastSocketError());
            return GG_ERROR_ERRNO((int)GetLastSocketError());
        }
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_Result
GG_Loop_PostMessage(GG_Loop* self, GG_LoopMessage* message, GG_Timeout timeout)
{
    // call the base implementation to post the message in the queue
    GG_Result result = GG_LoopBase_PostMessage(&self->base, message, timeout);
    if (GG_FAILED(result)) {
        return result;
    }

    // wake up the loop
    return GG_Loop_SendWakeup(self);
}

//----------------------------------------------------------------------
GG_Result
GG_Loop_InvokeSync(GG_Loop*            self,
                   GG_LoopSyncFunction function,
                   void*               function_argument,
                   int*                function_result)
{
    return GG_LoopBase_InvokeSync(&self->base, function, function_argument, function_result);
}

//----------------------------------------------------------------------
GG_Result
GG_Loop_InvokeAsync(GG_Loop*             self,
                    GG_LoopAsyncFunction function,
                    void*                function_argument)
{
    return GG_LoopBase_InvokeAsync(&self->base, function, function_argument);
}

//----------------------------------------------------------------------
GG_Result
GG_Loop_AddFileDescriptorHandler(GG_Loop*                           self,
                                 GG_LoopFileDescriptorEventHandler* handler)
{
    GG_ASSERT(self);

    // unlink in case this handler is already linked (should not happen)
    if (!GG_LINKED_LIST_NODE_IS_UNLINKED(&handler->base.list_node)) {
        GG_LINKED_LIST_NODE_REMOVE(&handler->base.list_node);
    }

    // add the handler to the monitors
    GG_LINKED_LIST_APPEND(&self->monitor_handlers, &handler->base.list_node);

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_Result
GG_Loop_RemoveFileDescriptorHandler(GG_Loop*                           self,
                                    GG_LoopFileDescriptorEventHandler* handler)
{
    GG_ASSERT(self);

    // check that this handler is linked
    if (GG_LINKED_LIST_NODE_IS_UNLINKED(&handler->base.list_node)) {
        return GG_SUCCESS;
    }

    // remove the handler from the monitors
    GG_LINKED_LIST_NODE_REMOVE(&handler->base.list_node);

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
static void
GG_Loop_OnEvent(GG_LoopEventHandler* _self, GG_Loop* loop)
{
    GG_ASSERT(_self);
    GG_COMPILER_UNUSED(loop);
    GG_Loop* self = GG_SELF_M(wakeup_handler.base, GG_Loop, GG_LoopEventHandler);

    // read all we can from the wakeup file descriptor, ignoring errors
    uint8_t msg;
    GG_ssize_t io_result;
    do {
        io_result = recv(self->wakeup_read_fd, (void*)&msg, 1, 0);
    } while (io_result > 0 || (GG_BSD_SOCKET_CALL_FAILED(io_result) && GetLastSocketError() == EINTR));

    // process all queued messages without waiting
    GG_Result result;
    unsigned int message_count = 0;
    do {
        result = GG_LoopBase_ProcessMessage(&self->base, 0);
        ++message_count;
    } while (GG_SUCCEEDED(result));
    GG_LOG_FINER("processed %d messages", (int)(message_count-1));
}

//----------------------------------------------------------------------
GG_TimerScheduler*
GG_Loop_GetTimerScheduler(GG_Loop* self)
{
    GG_ASSERT(self);
    return self->base.timer_scheduler;
}

//----------------------------------------------------------------------
void
GG_Loop_RequestTermination(GG_Loop* self)
{
    GG_THREAD_GUARD_CHECK_BINDING(&self->base);

    GG_LoopBase_RequestTermination(&self->base);
}

//----------------------------------------------------------------------
GG_LoopMessage*
GG_Loop_CreateTerminationMessage(GG_Loop* self)
{
    return GG_LoopBase_CreateTerminationMessage(&self->base);
}

/*----------------------------------------------------------------------
|   function table
+---------------------------------------------------------------------*/
GG_IMPLEMENT_INTERFACE(GG_Loop, GG_LoopEventHandler) {
    GG_Loop_OnEvent
};

//----------------------------------------------------------------------
#if GG_CONFIG_PLATFORM == GG_PLATFORM_WINDOWS
static GG_Result
SetNonBlocking(GG_SocketFd fd)
{
    u_long args = 1;
    if (ioctlsocket(fd, FIONBIO, &args)) {
        return GG_FAILURE;
    }
    return GG_SUCCESS;
}
#else
static GG_Result
SetNonBlocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    flags |= O_NONBLOCK;
    if (fcntl(fd, F_SETFL, flags)) {
        GG_LOG_WARNING("fcntl failed (%d)", errno);
        return GG_ERROR_ERRNO(errno);
    }
    return GG_SUCCESS;
}
#endif

//----------------------------------------------------------------------
static void
GG_Loop_CloseWakeupFds(GG_Loop* self)
{
    // close if open, and init values that shouldn't be 0
    if (!GG_BSD_SOCKET_IS_INVALID(self->wakeup_read_fd)) {
        close(self->wakeup_read_fd);
        self->wakeup_read_fd = GG_BSD_SOCKET_INVALID_HANDLE;
    }
#if defined(GG_CONFIG_ENABLE_PER_PID_SOCKETPAIR_FD)
    for (unsigned int i = 0; i < GG_ARRAY_SIZE(self->wakeup_write_fds); i++) {
        if (!GG_BSD_SOCKET_IS_INVALID(self->wakeup_write_fds[i].fd)) {
            close(self->wakeup_write_fds[i].fd);
            self->wakeup_write_fds[i].fd = GG_BSD_SOCKET_INVALID_HANDLE;
        }
        self->wakeup_write_fds[i].pid = GG_INVALID_PID_VALUE;
    }
#else
    if (!GG_BSD_SOCKET_IS_INVALID(self->wakeup_write_fd)) {
        close(self->wakeup_write_fd);
        self->wakeup_write_fd = GG_BSD_SOCKET_INVALID_HANDLE;
    }
#endif
}

//----------------------------------------------------------------------
#if defined(GG_CONFIG_ENABLE_BSD_SOCKETPAIR_EMULATION)
#include <string.h>

static GG_Result
GG_Loop_CreateWakeupFds(GG_Loop* self)
{
    GG_Result result = GG_SUCCESS;

    // init all the file descriptors so we don't risk leaving them in an undefined state
    self->wakeup_read_fd = GG_BSD_SOCKET_INVALID_HANDLE;
#if defined(GG_CONFIG_ENABLE_PER_PID_SOCKETPAIR_FD)
    for (unsigned int i = 0; i < GG_ARRAY_SIZE(self->wakeup_write_fds); i++) {
        self->wakeup_write_fds[i].fd = GG_BSD_SOCKET_INVALID_HANDLE;
        self->wakeup_write_fds[i].pid = GG_INVALID_PID_VALUE;
    }
#else
    self->wakeup_write_fd = GG_BSD_SOCKET_INVALID_HANDLE;
#endif

    // create two UDP sockets connected to each other
    GG_LOG_FINE("setting up UDP sockets as wakeup file descriptors");
    result = GG_Loop_CreateLoopbackSocket(&self->wakeup_read_fd);
    if (GG_FAILED(result)) {
        goto end;
    }
#if defined(GG_CONFIG_ENABLE_PER_PID_SOCKETPAIR_FD)
    result = GG_Mutex_Create(&self->wakeup_write_fds_lock);
#else
    result = GG_Loop_CreateLoopbackSocket(&self->wakeup_write_fd);
#endif
    if (GG_FAILED(result)) {
        goto end;
    }

    // read the port that was assigned to the read-side socket
    GG_socklen_t name_length = sizeof(self->wakeup_read_socket_address);
    int bsd_result = getsockname(self->wakeup_read_fd,
                                 (struct sockaddr*)&self->wakeup_read_socket_address,
                                 &name_length);
    if (bsd_result != 0) {
        result = GG_ERROR_ERRNO(GetLastSocketError());
        GG_LOG_WARNING("getsockname() failed (%d)", result);
        goto end;
    }

#if !defined(GG_CONFIG_ENABLE_PER_PID_SOCKETPAIR_FD)
    result = GG_Loop_ConnectLoopbackSocket(self, self->wakeup_write_fd);
    if (GG_FAILED(result)) {
        goto end;
    }
#endif

    // make the read-side fd non-blocking
    result = SetNonBlocking(self->wakeup_read_fd);

    // done
end:
    if (GG_FAILED(result)) {
#if defined(GG_CONFIG_ENABLE_PER_PID_SOCKETPAIR_FD)
        GG_Mutex_Destroy(self->wakeup_write_fds_lock);
        self->wakeup_write_fds_lock = NULL;
#endif
        GG_Loop_CloseWakeupFds(self);
    }

    return result;
}
#else
//----------------------------------------------------------------------
static GG_Result
GG_Loop_CreateWakeupFds(GG_Loop* self)
{
    // init all the file descriptors so we don't risk leaving them in an undefined state
    self->wakeup_read_fd  = GG_BSD_SOCKET_INVALID_HANDLE;
    self->wakeup_write_fd = GG_BSD_SOCKET_INVALID_HANDLE;

    // create a socket pair
    GG_SocketFd wakeup_fds[2];
    int socket_result = socketpair(AF_UNIX, SOCK_DGRAM, 0, wakeup_fds);
    if (socket_result != 0) {
        GG_LOG_WARNING("socketpair failed (%d)", errno);
        return GG_ERROR_ERRNO(errno);
    }

    // make the read-side fd non-blocking
    GG_Result result = SetNonBlocking(wakeup_fds[0]);
    if (GG_FAILED(result)) {
        close(wakeup_fds[0]);
        close(wakeup_fds[1]);
        return result;
    }

    // use the socket pair
    self->wakeup_read_fd  = wakeup_fds[0];
    self->wakeup_write_fd = wakeup_fds[1];

    return GG_SUCCESS;
}
#endif

//----------------------------------------------------------------------
GG_Result
GG_Loop_BindToCurrentThread(GG_Loop* self)
{
    return GG_LoopBase_BindToCurrentThread(&self->base);
}

//----------------------------------------------------------------------
GG_Result
GG_Loop_Create(GG_Loop** loop)
{
    GG_ASSERT(loop);

    // default return value
    *loop = NULL;

    // allocate a new object
    GG_Loop* self = (GG_Loop*)GG_AllocateZeroMemory(sizeof(GG_Loop));
    if (self == NULL) return GG_ERROR_OUT_OF_MEMORY;

    // init the base class
    GG_Result result = GG_LoopBase_Init(&self->base);
    if (GG_FAILED(result)) {
        goto end;
    }

    // init the inspectable interface
    GG_IF_INSPECTION_ENABLED(GG_SET_INTERFACE(&self->base, GG_Loop, GG_Inspectable));

    // init the monitor handler list
    GG_LINKED_LIST_INIT(&self->monitor_handlers);

    // try to allocate the wakeup file descriptors
    result = GG_Loop_CreateWakeupFds(self);
    if (GG_FAILED(result)) {
        goto end;
    }

    // register the wakeup handler
    self->wakeup_handler.fd = self->wakeup_read_fd;
    self->wakeup_handler.event_mask  = GG_EVENT_FLAG_FD_CAN_READ;
    GG_SET_INTERFACE(&self->wakeup_handler.base, GG_Loop, GG_LoopEventHandler);
    GG_Loop_AddFileDescriptorHandler(self, &self->wakeup_handler);

end:
    if (GG_SUCCEEDED(result)) {
        *loop = self;
    } else {
        GG_Loop_Destroy(self);
        *loop = NULL;
    }
    return result;
}

//----------------------------------------------------------------------
void
GG_Loop_Destroy(GG_Loop* self)
{
    if (self == NULL) return;

    // close file descriptors
    GG_Loop_CloseWakeupFds(self);

#if defined(GG_CONFIG_ENABLE_PER_PID_SOCKETPAIR_FD)
    GG_Mutex_Destroy(self->wakeup_write_fds_lock);
#endif

    // deinit the base class
    GG_LoopBase_Deinit(&self->base);

    // free the object memory
    GG_FreeMemory(self);
}

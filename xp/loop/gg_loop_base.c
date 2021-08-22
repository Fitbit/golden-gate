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
 * @date 2017-11-26
 *
 * @details
 *
 * Loop base class.
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include "xp/common/gg_logging.h"
#include "xp/common/gg_memory.h"
#include "xp/common/gg_port.h"
#include "xp/common/gg_queues.h"
#include "xp/common/gg_system.h"
#include "xp/common/gg_threads.h"
#include "xp/common/gg_types.h"
#include "xp/common/gg_utils.h"
#include "gg_loop_base.h"

/*----------------------------------------------------------------------
|   logging
+---------------------------------------------------------------------*/
GG_SET_LOCAL_LOGGER("gg.xp.loop.base")

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

//----------------------------------------------------------------------
GG_Timestamp
GG_LoopBase_UpdateTime(GG_LoopBase* self)
{
    // get the current system time in nanoseconds
    GG_Timestamp now = GG_System_GetCurrentTimestamp();

    uint32_t scheduler_time = now >= self->start_time ?
        (uint32_t)((now - self->start_time)/GG_NANOSECONDS_PER_MILLISECOND) : 0;
    GG_LOG_FINER("check timers - now = %u", (int)scheduler_time);
    GG_Result fire_count = GG_TimerScheduler_SetTime(self->timer_scheduler, scheduler_time);

    // compute how long it took
    if (fire_count > 0) {
        int64_t elapsed = GG_System_GetCurrentTimestamp() - now;
        GG_LOG_FINER("timers fired: %u, elapsed: %d ns", fire_count, (int)elapsed);
    }
    return now;
}

//----------------------------------------------------------------------
uint32_t
GG_LoopBase_CheckTimers(GG_LoopBase* self)
{
    // update the scheduler time
    GG_LoopBase_UpdateTime(self);

    // ask when the next scheduled timer is set to fire, relative to now
    uint32_t next_timer = GG_TimerScheduler_GetNextScheduledTime(self->timer_scheduler);

    // never wait for 0
    if (next_timer == 0) {
        next_timer = GG_LOOP_MIN_TIME_INTERVAL_MS;
    }

    return next_timer;
}

//----------------------------------------------------------------------
void
GG_LoopBase_RequestTermination(GG_LoopBase* self)
{
    self->termination_requested = true;
}

//----------------------------------------------------------------------
GG_LoopMessage*
GG_LoopBase_CreateTerminationMessage(GG_LoopBase* self)
{
    return GG_CAST(&self->termination_message, GG_LoopMessage);
}

//----------------------------------------------------------------------
static void
GG_LoopBaseTerminationMessage_Handle(GG_LoopMessage* _self)
{
    GG_LoopBase* self = GG_SELF_M(termination_message, GG_LoopBase, GG_LoopMessage);
    GG_LoopBase_RequestTermination(self);
}

//----------------------------------------------------------------------
static void
GG_LoopBaseTerminationMessage_Release(GG_LoopMessage* _self)
{
    GG_COMPILER_UNUSED(_self);
}

//----------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(GG_LoopBaseTerminationMessage, GG_LoopMessage) {
    .Handle  = GG_LoopBaseTerminationMessage_Handle,
    .Release = GG_LoopBaseTerminationMessage_Release
};

//----------------------------------------------------------------------
GG_Result
GG_LoopBase_PostMessage(GG_LoopBase* self, GG_LoopMessage* message, GG_Timeout timeout)
{
    // obtain a message item from the pool
    GG_LinkedListNode* item;
    GG_Result result = GG_SharedQueue_Dequeue(self->message_item_pool, &item, timeout);
    if (GG_FAILED(result)) {
        GG_LOG_SEVERE("GG_SharedQueue_Dequeue failed (%d)", result);
        return result;
    }

    // setup the message
    GG_LoopMessageItem* message_item = GG_LINKED_LIST_ITEM(item, GG_LoopMessageItem, list_node);
    message_item->message = message;

    // add to the queue (this should never fail, since the message item pool has the same
    // size as the max number of items in the message queue, so as we obtained this item
    // from the pool, there's some space in the queue)
    result = GG_SharedQueue_Enqueue(self->message_queue, item, 0);
    if (GG_FAILED(result)) {
        GG_LOG_SEVERE("GG_SharedQueue_Enqueue failed (%d)", result);
        GG_Result qresult = GG_SharedQueue_Enqueue(self->message_item_pool, item, 0);
        GG_COMPILER_UNUSED(qresult);
        GG_ASSERT(GG_SUCCEEDED(qresult));
        return result;
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_Result
GG_LoopBase_ProcessMessage(GG_LoopBase* self, GG_Timeout timeout)
{
    // wait for a message
    GG_LinkedListNode* item = NULL;
    GG_Result result = GG_SharedQueue_Dequeue(self->message_queue, &item, timeout);
    if (GG_FAILED(result)) {
        return result;
    }

    // update the timer scheduler now so that its notion of time is current
    GG_LoopBase_UpdateTime(self);

    // handle the message
    GG_ASSERT(item);
    GG_LoopMessageItem* message_item = GG_LINKED_LIST_ITEM(item, GG_LoopMessageItem, list_node);
    GG_LoopMessage_Handle(message_item->message);

    // we not longer need a reference to the message
    GG_LoopMessage_Release(message_item->message);
    message_item->message = NULL;

    // return the message item to the pool (should never fail)
    GG_SharedQueue_Enqueue(self->message_item_pool, item, 0);

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
static void
GG_LoopInvokeSyncMessage_Handle(GG_LoopMessage* _self)
{
    GG_LoopInvokeSyncMessage* self = GG_SELF(GG_LoopInvokeSyncMessage, GG_LoopMessage);

    // invoke the function
    GG_LOG_FINE("handling sync invoke message");
    self->function_result = self->function(self->function_argument);
}

//----------------------------------------------------------------------
static void
GG_LoopInvokeSyncMessage_Release(GG_LoopMessage* _self)
{
    GG_LoopInvokeSyncMessage* self = GG_SELF(GG_LoopInvokeSyncMessage, GG_LoopMessage);

    // wake up the sender that is waiting for the result
    GG_Semaphore_Release(self->result_semaphore);
}

//----------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(GG_LoopInvokeSyncMessage, GG_LoopMessage) {
    .Handle  = GG_LoopInvokeSyncMessage_Handle,
    .Release = GG_LoopInvokeSyncMessage_Release
};

//----------------------------------------------------------------------
GG_Result
GG_LoopBase_InvokeSync(GG_LoopBase*        self,
                       GG_LoopSyncFunction function,
                       void*               function_argument,
                       int*                function_result)
{
    GG_ASSERT(function);

    // check if we're on the loop thread
    if (GG_THREAD_GUARD_IS_CURRENT_THREAD_BOUND(self)) {
        // we're already on the loop thread, just call the method
        GG_LOG_FINE("invoking directly");
        int invoke_result = function(function_argument);

        // return the result to the caller
        if (function_result) {
            *function_result = invoke_result;
        }

        return GG_SUCCESS;
    }

    // we're on a different thread, wait until we're clear to send and wait
    GG_LOG_FINE("waiting for invoke mutex");
    GG_Result result = GG_Mutex_Lock(self->invoke_mutex);
    if (GG_FAILED(result)) {
        return result;
    }

    // initialize the invocation message
    self->invoke_message.function          = function;
    self->invoke_message.function_argument = function_argument;

    // post the message to the loop
    GG_LOG_FINE("posting function message to the loop");
    result = GG_Loop_PostMessage((GG_Loop*)self,
                                 GG_CAST(&self->invoke_message, GG_LoopMessage),
                                 GG_TIMEOUT_INFINITE);
    if (GG_FAILED(result)) {
        GG_Mutex_Unlock(self->invoke_mutex);
        return result;
    }

    // wait for the result to be ready
    GG_LOG_FINE("waiting for the invoke result");
    GG_Semaphore_Acquire(self->invoke_message.result_semaphore);
    GG_LOG_FINE("got result = %d", self->invoke_message.function_result);

    // return the result to the caller
    if (function_result) {
        *function_result = self->invoke_message.function_result;
    }
    GG_Mutex_Unlock(self->invoke_mutex);
    return result;
}

//----------------------------------------------------------------------
static void
GG_LoopInvokeAsyncMessage_Handle(GG_LoopMessage* _self)
{
    GG_LoopInvokeAsyncMessage* self = GG_SELF(GG_LoopInvokeAsyncMessage, GG_LoopMessage);

    // invoke the function
    GG_LOG_FINE("handling async invoke message");
    self->function(self->function_argument);
}

//----------------------------------------------------------------------
static void
GG_LoopInvokeAsyncMessage_Release(GG_LoopMessage* self)
{
    GG_FreeMemory(self);
}

//----------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(GG_LoopInvokeAsyncMessage, GG_LoopMessage) {
    .Handle  = GG_LoopInvokeAsyncMessage_Handle,
    .Release = GG_LoopInvokeAsyncMessage_Release
};

//----------------------------------------------------------------------
GG_Result
GG_LoopBase_InvokeAsync(GG_LoopBase*         self,
                        GG_LoopAsyncFunction function,
                        void*                function_argument)
{
    // allocate a new message
    GG_LoopInvokeAsyncMessage* message = GG_AllocateZeroMemory(sizeof(GG_LoopInvokeAsyncMessage));
    if (message == NULL) {
        return GG_ERROR_OUT_OF_MEMORY;
    }

    // initialize the message
    message->function          = function;
    message->function_argument = function_argument;
    GG_SET_INTERFACE(message, GG_LoopInvokeAsyncMessage, GG_LoopMessage);

    // post the message to the loop
    return GG_Loop_PostMessage((GG_Loop*)self, GG_CAST(message, GG_LoopMessage), GG_TIMEOUT_INFINITE);
}

//----------------------------------------------------------------------
GG_Result
GG_LoopBase_BindToCurrentThread(GG_LoopBase* self)
{
    // check if we're already bound
    if (GG_THREAD_GUARD_IS_OBJECT_BOUND(self)) {
        GG_LOG_WARNING("attempt to bind an already-bound loop");
        return GG_ERROR_INVALID_STATE;
    }

    // bind the loop to the current thread
    GG_THREAD_GUARD_BIND(self);

    // use the current thread for the "main loop" thread guard
    GG_ThreadGuard_SetMainLoopThreadId(GG_GetCurrentThreadId());

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_Result
GG_LoopBase_Init(GG_LoopBase* self)
{
    GG_Result result;

    // create the queues
    result = GG_SharedQueue_Create(GG_CONFIG_LOOP_MESSAGE_QUEUE_LENGTH, &self->message_item_pool);
    if (GG_FAILED(result)) {
        return result;
    }
    result = GG_SharedQueue_Create(GG_CONFIG_LOOP_MESSAGE_QUEUE_LENGTH, &self->message_queue);
    if (GG_FAILED(result)) {
        return result;
    }

    // stuff the message item pool
    for (unsigned int i = 0; i < GG_CONFIG_LOOP_MESSAGE_QUEUE_LENGTH; i++) {
        GG_SharedQueue_Stuff(self->message_item_pool, &self->message_items[i].list_node);
    }

    // create a timer scheduler
    result = GG_TimerScheduler_Create(&self->timer_scheduler);
    if (GG_FAILED(result)) {
        return result;
    }

    // init the timer scheduler to the current system time
    self->start_time = GG_System_GetCurrentTimestamp();
    GG_TimerScheduler_SetTime(self->timer_scheduler, 0);

    // create the sync invoke mutex and semaphore, and init the message handler vtable
    result = GG_Mutex_Create(&self->invoke_mutex);
    if (GG_FAILED(result)) {
        return result;
    }
    result = GG_Semaphore_Create(0, &self->invoke_message.result_semaphore);
    if (GG_FAILED(result)) {
        return result;
    }
    GG_SET_INTERFACE(&self->invoke_message, GG_LoopInvokeSyncMessage, GG_LoopMessage);

    // init the termination message
    GG_SET_INTERFACE(&self->termination_message, GG_LoopBaseTerminationMessage, GG_LoopMessage);
    return result;
}

//----------------------------------------------------------------------
void
GG_LoopBase_Deinit(GG_LoopBase* self)
{
    if (self == NULL) return;

    // release messages that are still in the queue
    GG_Result result;
    if (self->message_queue) {
        do {
            // dequeue without waiting
            GG_LinkedListNode* node = NULL;
            result = GG_SharedQueue_Dequeue(self->message_queue, &node, 0);
            if (GG_SUCCEEDED(result) && node) {
                GG_LoopMessage* message = GG_LINKED_LIST_ITEM(node, GG_LoopMessageItem, list_node)->message;
                GG_LoopMessage_Release(message);
            }
        } while (GG_SUCCEEDED(result));
    }

    // destroy allocated resources
    GG_SharedQueue_Destroy(self->message_item_pool);
    GG_SharedQueue_Destroy(self->message_queue);
    GG_TimerScheduler_Destroy(self->timer_scheduler);
    GG_Mutex_Destroy(self->invoke_mutex);
    GG_Semaphore_Destroy(self->invoke_message.result_semaphore);
}

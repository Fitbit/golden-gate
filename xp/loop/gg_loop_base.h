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
 * Base class for loop implementations.
 */

#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include "xp/common/gg_common.h"
#include "xp/loop/gg_loop.h"

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/
#if !defined(GG_CONFIG_LOOP_MESSAGE_QUEUE_LENGTH)
#define GG_CONFIG_LOOP_MESSAGE_QUEUE_LENGTH 64
#endif

#define GG_LOOP_MIN_TIME_INTERVAL_MS  1

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
/**
 * Item in a loop's message queue.
 */
typedef struct {
    GG_LinkedListNode list_node;
    GG_LoopMessage*   message;
} GG_LoopMessageItem;

typedef struct {
    GG_IMPLEMENTS(GG_LoopMessage);
    GG_LoopSyncFunction function;
    void*               function_argument;
    int                 function_result;
    GG_Semaphore*       result_semaphore; ///< used to wait-for/signal the result of the invoked function
} GG_LoopInvokeSyncMessage;

typedef struct {
    GG_IMPLEMENTS(GG_LoopMessage);
    GG_LoopAsyncFunction function;
    void*                function_argument;
} GG_LoopInvokeAsyncMessage;

/**
 * Base class that may be used by concrete loop implementations/ports.
 */
typedef struct {
    GG_IF_INSPECTION_ENABLED(GG_IMPLEMENTS(GG_Inspectable);)

    GG_Timestamp             start_time;
    GG_TimerScheduler*       timer_scheduler;
    GG_SharedQueue*          message_queue;
    GG_SharedQueue*          message_item_pool;       ///< blank messages in a pool, ready to be posted
    GG_LoopMessageItem       message_items[GG_CONFIG_LOOP_MESSAGE_QUEUE_LENGTH];
    GG_Mutex*                invoke_mutex;            ///< used to serialize access to the sync invocation function
    GG_LoopInvokeSyncMessage invoke_message;          ///< used to post a sync invocation message
    bool                     termination_requested;
    struct {
        GG_IMPLEMENTS(GG_LoopMessage);
    }                        termination_message;

    GG_THREAD_GUARD_ENABLE_BINDING
} GG_LoopBase;

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/
/**
 * Update the timer scheduler notion of current time
 */
void GG_LoopBase_UpdateTime(GG_LoopBase* self);

/**
 * Check for expired timers and call timer listeners.
 */
uint32_t GG_LoopBase_CheckTimers(GG_LoopBase* self);

/**
 * Post a message to the loop's queue, from a thread other than the loop thread.
 */
GG_Result GG_LoopBase_PostMessage(GG_LoopBase* self, GG_LoopMessage* message, GG_Timeout timeout);

/**
 * Process the next message in the queue.
 *
 * @param self The object on which this method is invoked.
 * @param timeout Maximum time to wait for a message.
 *
 * @return #GG_SUCCESS if a message was processed, #GG_ERROR_TIMEOUT if no message
 * arrived before the timeout elapsed, or a negative error code.
 */
GG_Result GG_LoopBase_ProcessMessage(GG_LoopBase* self, GG_Timeout timeout);

GG_Result GG_LoopBase_InvokeSync(GG_LoopBase*        self,
                                 GG_LoopSyncFunction function,
                                 void*               function_argument,
                                 int*                function_result);

GG_Result GG_LoopBase_InvokeAsync(GG_LoopBase*         self,
                                  GG_LoopAsyncFunction function,
                                  void*                function_argument);

/**
 * @see GG_Loop_BindToCurrentThread
 */
GG_Result
GG_LoopBase_BindToCurrentThread(GG_LoopBase* self);

/**
 * Initialize an instance.
 */
GG_Result GG_LoopBase_Init(GG_LoopBase* self);

/**
 * Deinitialize an instance.
 */
void GG_LoopBase_Deinit(GG_LoopBase* self);

/**
 * Base implementation of GG_Loop_RequestTermination
 */
void GG_LoopBase_RequestTermination(GG_LoopBase* self);

/**
 * Base implementation of GG_Loop_CreateTerminationMessage
 */
GG_LoopMessage* GG_LoopBase_CreateTerminationMessage(GG_LoopBase* self);

#if defined(__cplusplus)
}
#endif

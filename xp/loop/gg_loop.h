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
 * @date 2017-09-18
 *
 * @details
 *
 * Loop interface.
 */

#pragma once

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdint.h>

#include "xp/common/gg_common.h"

//! @addtogroup Loop Loop
//! Loop functionality
//! @{

#if defined(__cplusplus)
extern "C" {
#endif

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/
#define GG_LOOP_DATA_SINK_PROXY_MAX_QUEUE_LENGTH    64

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
typedef struct GG_Loop GG_Loop;

/**
 * A GG_DataSinkProxy is an object that implements the GG_DataSink interface
 * as a proxy for an object that implements the GG_DataSink interface, in
 * a way that makes it possible to call GG_DataSink methods on the proxy
 * from a thread other than the GG_Loop thread on which the proxy'ed object
 * is intended to run.
 */
typedef struct GG_LoopDataSinkProxy GG_LoopDataSinkProxy;

/**
 * A GG_DataSinkListenerProxy is an object that implements the GG_DataSinkListener interface
 * as a proxy for an object that implements the GG_DataSinkListener interface, in
 * a way that makes it possible to call GG_DataSinkListener methods on the proxy
 * from a thread other than the GG_Loop thread on which the proxy'ed object
 * is intended to run.
 */
typedef struct GG_LoopDataSinkListenerProxy GG_LoopDataSinkListenerProxy;

GG_DECLARE_INTERFACE(GG_LoopEventHandler) {
    void (*OnEvent)(GG_LoopEventHandler* self, GG_Loop* loop);
};

GG_DECLARE_INTERFACE(GG_Runnable) {
    void (*Run)(GG_Runnable* self);
};

GG_DECLARE_INTERFACE(GG_LoopMessage) {
    void (*Handle)(GG_LoopMessage* self);
    void (*Release)(GG_LoopMessage* self);
};

typedef struct {
    GG_IMPLEMENTS(GG_LoopEventHandler);

    GG_LinkedListNode list_node;
} GG_LoopEventHandlerItem;

/**
 * Function that can be used with GG_Loop_InvokeSync
 */
typedef int (*GG_LoopSyncFunction)(void* arg);

/**
 * Function that can be used with GG_Loop_InvokeAsync
 */
typedef void (*GG_LoopAsyncFunction)(void* arg);

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

/**
 Create a new GG_Loop object.

 @param loop Address of the variable in which a pointer to the the new object is returned.
 @return GG_SUCCESS if the call succeeded, or an error code if it failed.
 */
GG_Result GG_Loop_Create(GG_Loop** loop);

/**
 Destroy a GG_Loop object.

 @param self The object to destroy.
 */
void GG_Loop_Destroy(GG_Loop* self);

/**
 * Bind a loop to the current thread.
 * NOTE: while this function may be called from any thread, it isn't re-entrant, so
 * the caller must ensure that no other methods may be invoked at the same time
 * by another thread.
 *
 * @param self The object on which this method is invoked.
 * @return GG_SUCCESS if the loop could be bound, or a negative error code.
 */
GG_Result GG_Loop_BindToCurrentThread(GG_Loop* self);

/**
 Run a loop.
 This method does not return until the loop is terminated.
 NOTE: if the loop isn't bound to a thread, this will automatically bind
 it to the current thread.

 @param self The loop to run.
 @return GG_SUCCESS if the loop was terminated cleanly, or an error code if the loop could not be run.
 */
GG_Result GG_Loop_Run(GG_Loop* self);

/**
 Do some work on the loop, with a maximum wait time.
 All non-blocking work will be performed, then this method will wait for
 more work to perform up to max_wait_time, and return.

 @param self The object on which this method is invoked.
 @param max_wait_time Maximum time to block for, in milliseconds.
 @param call_again_time Maximum time, in milliseconds, that the caller may wait until
 calling this method again. This argument may be NULL if max_wait_time is not 0.

 @return GG_SUCCESS if the call was successful, GG_ERROR_INTERRUPTED if
 loop termination was requested (in which case this method should not be
 called anymore), or another negative error code.
 */
GG_Result GG_Loop_DoWork(GG_Loop* self, uint32_t max_wait_time, uint32_t* call_again_time);

/**
 Request that the loop terminate cleanly as soon as possible.

 @param self The object on which this method is invoked.
 */
void GG_Loop_RequestTermination(GG_Loop* self);

/**
 Ask the loop for a termination message that can be used to send
 using GG_Loop_PostMessage in order to trigger a loop termination
 from outside the context of the loop thread.

 @param self The object on which this method is invoked.
 @return A GG_LoopMessage object that can be posted to the loop.
 */
GG_LoopMessage* GG_Loop_CreateTerminationMessage(GG_Loop* self);


/**
 Get the timer scheduler associated with a loop.

 @param self The object on which this method is invoked.
 @return The timer scheduler associated with the loop.
 */
GG_TimerScheduler* GG_Loop_GetTimerScheduler(GG_Loop* self);

/**
 * Post a message to the loop thread.
 * The message will he handled in the context of the loop thread.
 * This function can be called from a different thread than the loop thread!
 * NOTE: the ownership of the message is transferred to the loop.
 *
 * @param self The object on which this method is invoked.
 * @param message The message to post.
 * @param timeout The maximum amount of time to wait for space to be available
 * in the message queue.
 *
 * @return GG_SUCCESS if the message was posted, or a negative error code.
 *
 */
GG_Result GG_Loop_PostMessage(GG_Loop* self, GG_LoopMessage* message, GG_Timeout timeout);

/**
 * Request the invocation of a function in the context of a loop thread.
 * This is a synchronous invocation, so the caller will block until the function
 * has been invoked, and can obtain the return value of the invoked function.
 * Because this is a synchronous invocation, the invoked function argument may
 * point directly or indirectly to stack variables.
 *
 * @param self The object on which this method is called.
 * @param function The function to invoke.
 * @param function_argument Argument that will be passed to the invoked function.
 * @param function_result Pointer to the variable in which the function result will be returned
 *
 * @return GG_SUCCESS if the function could be invoked, or a negative error code.
 */
GG_Result GG_Loop_InvokeSync(GG_Loop*            self,
                             GG_LoopSyncFunction function,
                             void*               function_argument,
                             int*                function_result);

/**
 * Request the invocation of a function in the context of a loop thread.
 * This is an asynchronous invocation, so the call will return immediately
 * without waiting for the function invocation to occur. The function invocation
 * may happen before or after this method call returns, depending on how the
 * calling thread and the loop thread are scheduled as well as other
 * factors.
 * Since this is an asynchronous invocation, the function arguments must
 * not point directly or indirectly to stack variables, because the caller's stack
 * may no longer be valid when the function is invoked.
 *
 * @param self The object on which this method is invoked.
 * @param function The function to invoke.
 * @param function_argument Argument that will be passed to the invoked function.
 *
 * @return GG_SUCCESS if the invocation request could be sent, or a negative
 * error code.
 */
GG_Result GG_Loop_InvokeAsync(GG_Loop*             self,
                              GG_LoopAsyncFunction function,
                              void*                function_argument);

/**
 Create a cross-thread proxy for an object that implements the GG_DataSink interface.

 @param self The GG_Loop object in which the proxy'ed object is intended to run.
 @param queue_size Maximum number of items that can be queued (between 0 and GG_LOOP_DATA_SINK_PROXY_MAX_QUEUE_LENGTH).
 @param sink The GG_DataSink object to proxy.
 @param sink_proxy Address of the variable where the created proxy object is returned.
 @return GG_SUCCESS or an error code if the call fails.
 */
GG_Result GG_Loop_CreateDataSinkProxy(GG_Loop*               self,
                                      unsigned int           queue_size,
                                      GG_DataSink*           sink,
                                      GG_LoopDataSinkProxy** sink_proxy);

/**
 * Obtain a GG_Inspectable interface for a loop.
 *
 * @param self The object on which this method is invoked.
 * @return The object's GG_Inspectable interface.
 */
GG_Inspectable* GG_Loop_AsInspectable(GG_Loop* self);

/**
 Destroy a GG_DataSinkProxy object.

 @param self The object to destroy.
 */
void GG_LoopDataSinkProxy_Destroy(GG_LoopDataSinkProxy* self);

/**
 Obtain the GG_DataSink interface of a GG_LoopDataSinkProxy object.

 @param self The object for which to obtain the interface.
 @return the GG_DataSink interface for the object.
 */
GG_DataSink* GG_LoopDataSinkProxy_AsDataSink(GG_LoopDataSinkProxy* self);

/**
 Create a cross-thread proxy for an object that implements the GG_DataSinkListener interface.

 @param self The GG_Loop object in which the proxy'ed object is intended to run.
 @param listener The GG_DataSinkListener object to proxy.
 @param listener_proxy Address of the variable where the created proxy object is returned.
 @return GG_SUCCESS or an error code if the call fails.
 */
GG_Result GG_Loop_CreateDataSinkListenerProxy(GG_Loop*                       self,
                                              GG_DataSinkListener*           listener,
                                              GG_LoopDataSinkListenerProxy** listener_proxy);

/**
 Destroy a GG_DataSinkListenerProxy object.

 @param self The object to destroy.
 */
void GG_LoopDataSinkListenerProxy_Destroy(GG_LoopDataSinkListenerProxy* self);

/**
 Obtain the GG_DataSinkListener interface of a GG_LoopDataSinkListenerProxy object.

 @param self The object for which to obtain the interface.
 @return the GG_DataSinkListener interface for the object.
 */
GG_DataSinkListener* GG_LoopDataSinkListenerProxy_AsDataSinkListener(GG_LoopDataSinkListenerProxy* self);

/**
 Thunk function for the GG_LoopEventHandler::OnEvent method.

 @param self The object on which the method is called.
 @param loop The GG_Loop object that emits the event.
 */
void GG_LoopEventHandler_OnEvent(GG_LoopEventHandler* self, GG_Loop* loop);

/**
 Thunk function for the GG_LoopMessage::Handle method

 @param self The object on which the method is called.
 */
void GG_LoopMessage_Handle(GG_LoopMessage* self);

/**
 Thunk function for the GG_LoopMessage::Release method

 @param self The object on which the method is called.
 */
void GG_LoopMessage_Release(GG_LoopMessage* self);

//! @}

#if defined(__cplusplus)
}
#endif

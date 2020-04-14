/**
 *
 * @file
 *
 * @copyright
 * Copyright 2017 by Fitbit, Inc., all rights reserved.
 *
 * @author Gilles Boccon-Gibod
 *
 * @date 2017-11-26
 *
 * @details
 *
 * Generic Event Loop implementation.
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include "xp/common/gg_common.h"
#include "xp/loop/gg_loop.h"
#include "xp/loop/gg_loop_base.h"

/*----------------------------------------------------------------------
|   logging
+---------------------------------------------------------------------*/
GG_SET_LOCAL_LOGGER("gg.xp.loop.generic")

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
struct GG_Loop {
    // inherited base class
    GG_LoopBase base;
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
GG_Loop_Inspect(GG_Inspectable* self, GG_Inspector* inspector, const GG_InspectionOptions* options)
{
    GG_COMPILER_UNUSED(self);
    GG_COMPILER_UNUSED(inspector);
    GG_COMPILER_UNUSED(options);
    return GG_SUCCESS;
}

GG_IMPLEMENT_INTERFACE(GG_Loop, GG_Inspectable) {
    .Inspect = GG_Loop_Inspect
};
#endif

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

    // loop until terminated
    self->base.termination_requested = false;
    for (;;) {
        GG_Result result = GG_Loop_DoWork(self, GG_TIMER_NEVER, NULL);
        if (GG_FAILED(result)) {
            if (result == GG_ERROR_INTERRUPTED) {
                // not an error, just return
                break;
            } else {
                return result;
            }
        }
    }
    GG_LOG_INFO("loop terminating");

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_Result
GG_Loop_DoWork(GG_Loop* self, uint32_t max_wait_time, uint32_t* call_again_time)
{
    GG_ASSERT(max_wait_time || call_again_time);

    // set a default value for out parameters
    if (call_again_time) {
        *call_again_time = 0; // the caller should try again immediately
    }

    // check that the loop isn't already terminated
    if (self->base.termination_requested) {
        return GG_ERROR_INTERRUPTED;
    }

    // check if there's a message ready to be processed without blocking
    GG_Result result = GG_LoopBase_ProcessMessage(&self->base, 0);
    if (GG_SUCCEEDED(result)) {
        GG_LOG_FINER("processed one message");
        return self->base.termination_requested ? GG_ERROR_INTERRUPTED : GG_SUCCESS;
    } else if (result != GG_ERROR_TIMEOUT) {
        // something went wrong
        GG_LOG_SEVERE("error while processing message (%d)", result);
        return result;
    }

    // process all timers
    uint32_t next_timer = GG_LoopBase_CheckTimers(&self->base);

    // check for termination in case a timer handler requested it
    if (self->base.termination_requested) {
        return GG_ERROR_INTERRUPTED;
    }

    // wait for some more work if needed
    max_wait_time = GG_MIN(max_wait_time, next_timer);
    if (max_wait_time) {
        // wait for I/O or a message
        GG_LOG_FINER("waiting for a message, up to %u ms", (int)max_wait_time);
        GG_Timeout timeout = max_wait_time == GG_TIMER_NEVER ?
                             GG_TIMEOUT_INFINITE :
                             ((GG_Timeout)max_wait_time) * GG_NANOSECONDS_PER_MILLISECOND;
        result = GG_LoopBase_ProcessMessage(&self->base, timeout);
        if (GG_SUCCEEDED(result)) {
            // a message was processed, the caller can try again without waiting
            GG_LOG_FINER("processed one message");
        } else {
            if (result != GG_ERROR_TIMEOUT) {
                // something went wrong
                GG_LOG_SEVERE("error while waiting for message (%d)", result);
                return result;
            }
        }
    } else {
        if (call_again_time) {
            *call_again_time = next_timer;
        }
    }

    return self->base.termination_requested ? GG_ERROR_INTERRUPTED : GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_Result
GG_Loop_PostMessage(GG_Loop* self, GG_LoopMessage* message, GG_Timeout timeout)
{
    // call the base implementation to post the message in the queue
    return GG_LoopBase_PostMessage(&self->base, message, timeout);
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
                    void*               function_argument)
{
    return GG_LoopBase_InvokeAsync(&self->base, function, function_argument);
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
        GG_Loop_Destroy(self);
        return result;
    }

    // init the inspectable interface
    GG_IF_INSPECTION_ENABLED(GG_SET_INTERFACE(&self->base, GG_Loop, GG_Inspectable));

    // return the new object
    *loop = self;

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
void
GG_Loop_Destroy(GG_Loop* self)
{
    if (self == NULL) return;

    // deinit the base class
    GG_LoopBase_Deinit(&self->base);

    // free the object memory
    GG_FreeMemory(self);
}

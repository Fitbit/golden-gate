/**
 *
 * @file
 *
 * @copyright
 * Copyright 2017 by Fitbit, Inc., all rights reserved.
 *
 * @author Gilles Boccon-Gibod
 *
 * @date 2017-12-08
 *
 * @details
 * Asynchronous source/sink pipe
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <string.h>

#include "xp/common/gg_types.h"
#include "xp/common/gg_utils.h"
#include "xp/common/gg_memory.h"
#include "xp/common/gg_buffer.h"
#include "xp/common/gg_io.h"
#include "xp/common/gg_lists.h"
#include "gg_async_pipe.h"

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
struct GG_AsyncPipe {
    GG_IMPLEMENTS(GG_DataSource);
    GG_IMPLEMENTS(GG_DataSink);
    GG_IMPLEMENTS(GG_DataSinkListener);
    GG_IMPLEMENTS(GG_TimerListener);

    GG_Timer*             timer;
    GG_DataSink*          sink;
    GG_DataSinkListener*  sink_listener;
    unsigned int          max_items;
    GG_Buffer**           items;
    unsigned int          item_count;
};

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

//----------------------------------------------------------------------
static GG_Result
GG_AsyncPipe_PutData(GG_DataSink* _self, GG_Buffer* data, const GG_BufferMetadata* metadata)
{
    GG_AsyncPipe* self = GG_SELF(GG_AsyncPipe, GG_DataSink);
    GG_COMPILER_UNUSED(metadata);

    // check that we don't exceed the queue length
    if (self->item_count == self->max_items) {
        return GG_ERROR_WOULD_BLOCK;
    }

    // keep a reference to the buffer for later delivery
    self->items[self->item_count] = GG_Buffer_Retain(data);
    ++self->item_count;

    // arm the timer so that we can deliver the data later
    if (!GG_Timer_IsScheduled(self->timer)) {
        GG_Timer_Schedule(self->timer, GG_CAST(self, GG_TimerListener), 0);
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
static GG_Result
GG_AsyncPipe_SetListener(GG_DataSink* _self, GG_DataSinkListener* listener)
{
    GG_AsyncPipe* self = GG_SELF(GG_AsyncPipe, GG_DataSink);

    self->sink_listener = listener;

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
static GG_Result
GG_AsyncPipe_SetDataSink(GG_DataSource* _self, GG_DataSink* sink)
{
    GG_AsyncPipe* self = GG_SELF(GG_AsyncPipe, GG_DataSource);

    // de-register as a listener from the current sink
    if (self->sink) {
        GG_DataSink_SetListener(self->sink, NULL);
    }

    // keep a reference to the new sink
    self->sink = sink;

    // register as a listener
    if (sink) {
        GG_DataSink_SetListener(sink, GG_CAST(self, GG_DataSinkListener));
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
static void
GG_AsyncPipe_OnCanPut(GG_DataSinkListener* _self)
{
    GG_AsyncPipe* self = GG_SELF(GG_AsyncPipe, GG_DataSinkListener);

    // check that we have a sink
    if (!self->sink) {
        return;
    }

    // deliver any pending data to the sink until it would block
    while (self->item_count) {
        GG_Buffer *buffer = self->items[0];
        GG_Result result = GG_DataSink_PutData(self->sink, buffer, NULL);
        if (GG_SUCCEEDED(result)) {
            GG_Buffer_Release(buffer);
            --self->item_count;

            // Relocate remaining items
            for (unsigned int i=0; i < self->item_count; i++) {
                self->items[i] = self->items[i+1];
            }
        } else {
            // if the data couldn't be delivered, we're done
            break;
        }
    }

    // there's space available, notify the listener
    if (self->sink_listener && self->item_count < self->max_items) {
        GG_DataSinkListener_OnCanPut(self->sink_listener);
    }
}

//----------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(GG_AsyncPipe, GG_DataSink) {
    .PutData     = GG_AsyncPipe_PutData,
    .SetListener = GG_AsyncPipe_SetListener
};

GG_IMPLEMENT_INTERFACE(GG_AsyncPipe, GG_DataSinkListener) {
    .OnCanPut = GG_AsyncPipe_OnCanPut
};

GG_IMPLEMENT_INTERFACE(GG_AsyncPipe, GG_DataSource) {
    .SetDataSink = GG_AsyncPipe_SetDataSink
};

//----------------------------------------------------------------------
static void
GG_AsyncPipe_OnTimerFired(GG_TimerListener* _self, GG_Timer* timer, uint32_t elapsed)
{
    GG_AsyncPipe* self = (GG_AsyncPipe*)GG_SELF(GG_AsyncPipe, GG_TimerListener);
    GG_COMPILER_UNUSED(timer);
    GG_COMPILER_UNUSED(elapsed);

    // try to deliver pending data
    GG_AsyncPipe_OnCanPut(GG_CAST(self, GG_DataSinkListener));

    // if the data has been delivered, notify the listener they can try to put again
    if (self->item_count == 0 && self->sink_listener) {
        GG_DataSinkListener_OnCanPut(self->sink_listener);
    }
}

//----------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(GG_AsyncPipe, GG_TimerListener) {
    .OnTimerFired = GG_AsyncPipe_OnTimerFired
};

//----------------------------------------------------------------------
GG_Result
GG_AsyncPipe_Create(GG_TimerScheduler* timer_scheduler, unsigned int max_items, GG_AsyncPipe** pipe)
{
    GG_ASSERT(max_items > 0);

    // allocate a new object
    *pipe = (GG_AsyncPipe*)GG_AllocateZeroMemory(sizeof(GG_AsyncPipe));
    if (*pipe == NULL) {
        return GG_ERROR_OUT_OF_MEMORY;
    }

    // init the object fields
    GG_Result result = GG_TimerScheduler_CreateTimer(timer_scheduler, &(*pipe)->timer);
    if (GG_FAILED(result)) {
        GG_FreeMemory(*pipe);
        return result;
    }
    (*pipe)->items = GG_AllocateMemory(sizeof(GG_Buffer*)*max_items);
    if ((*pipe)->items == NULL) {
        GG_FreeMemory(*pipe);
        return GG_ERROR_OUT_OF_MEMORY;
    }
    (*pipe)->item_count = 0;
    (*pipe)->max_items = max_items;

    // initialize the object interfaces
    GG_SET_INTERFACE(*pipe, GG_AsyncPipe, GG_DataSource);
    GG_SET_INTERFACE(*pipe, GG_AsyncPipe, GG_DataSink);
    GG_SET_INTERFACE(*pipe, GG_AsyncPipe, GG_DataSinkListener);
    GG_SET_INTERFACE(*pipe, GG_AsyncPipe, GG_TimerListener);

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
void
GG_AsyncPipe_Destroy(GG_AsyncPipe* self)
{
    if (self == NULL) return;

    // de-register as a listener from the sink
    if (self->sink) {
        GG_DataSink_SetListener(self->sink, NULL);
    }

    // cancel the timer
    GG_Timer_Destroy(self->timer);

    // release any buffer we may have
    if (self->items) {
        for (unsigned int i=0; i < self->item_count; i++) {
            GG_Buffer_Release(self->items[i]);
        }

        GG_FreeMemory(self->items);
    }

    // free the object memory
    GG_ClearAndFreeObject(self, 4);
}

//----------------------------------------------------------------------
GG_DataSource*
GG_AsyncPipe_AsDataSource(GG_AsyncPipe* self)
{
    return GG_CAST(self, GG_DataSource);
}

//----------------------------------------------------------------------
GG_DataSink*
GG_AsyncPipe_AsDataSink(GG_AsyncPipe* self)
{
    return GG_CAST(self, GG_DataSink);
}

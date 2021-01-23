/**
 *
 * @file
 *
 * @copyright
 * Copyright 2017-2020 Fitbit, Inc
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author Sylvain Rebaud
 *
 * @date 2019-04-02
 *
 * @details
 * Monitor data source
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <string.h>

#include "xp/common/gg_logging.h"
#include "xp/common/gg_memory.h"
#include "xp/common/gg_port.h"
#include "xp/common/gg_system.h"
#include "xp/common/gg_types.h"
#include "xp/common/gg_utils.h"
#include "gg_activity_data_monitor.h"

/*----------------------------------------------------------------------
|   logging
+---------------------------------------------------------------------*/
GG_SET_LOCAL_LOGGER("gg.xp.utils.activity-data-monitor")

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
struct GG_ActivityDataMonitor {
    GG_IMPLEMENTS(GG_DataSource);
    GG_IMPLEMENTS(GG_DataSink);
    GG_IMPLEMENTS(GG_DataSinkListener);
    GG_IMPLEMENTS(GG_TimerListener);
    GG_IF_INSPECTION_ENABLED(GG_IMPLEMENTS(GG_Inspectable);)

    GG_EventEmitterBase  event_emitter;
    uint32_t             direction;              ///< Direction
    bool                 active;                 ///< True if activity was last detected
    GG_DataSink*         sink;                   ///< Sink to forward data to
    GG_DataSinkListener* sink_listener;          ///< Sink Listener to forward onCanPut calls to
    GG_Timer*            inactivity_timer;       ///< Inactivity timer
    uint32_t             inactivity_timeout;     ///< Inactivity timer interval, in ms
};

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

//----------------------------------------------------------------------
static void
GG_ActivityDataMonitor_NotifyActivityChange(GG_ActivityDataMonitor* self, GG_Timestamp time)
{
    // notify the event listener if we have one
    if (self->event_emitter.listener) {
        GG_ActivityMonitorChangeEvent event = {
            .base   = {
                .type   = GG_EVENT_TYPE_ACTIVITY_MONITOR_CHANGE,
                .source = self
            },
            .direction     = self->direction,
            .active        = self->active,
            .detected_time = time
        };

        GG_EventListener_OnEvent(self->event_emitter.listener, &event.base);
    }
}

//----------------------------------------------------------------------
static GG_Result
GG_ActivityDataMonitor_SetDataSink(GG_DataSource* _self, GG_DataSink* sink)
{
    GG_ActivityDataMonitor* self = GG_SELF(GG_ActivityDataMonitor, GG_DataSource);

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
static GG_Result
GG_ActivityDataMonitor_PutData(GG_DataSink* _self, GG_Buffer* data, const GG_BufferMetadata* metadata)
{
    GG_ActivityDataMonitor* self   = GG_SELF(GG_ActivityDataMonitor, GG_DataSink);
    GG_Result               result = GG_SUCCESS;

    if (self->sink) {
        result = GG_DataSink_PutData(self->sink, data, metadata);
    }

    // notify immediately if detected activity after period of inactivity
    if (!self->active) {
        GG_LOG_FINER("%s data activity detected.",
            self->direction == GG_ACTIVITY_MONITOR_DIRECTION_BOTTOM_TO_TOP ? "Incoming" : "Outgoing");

        self->active = true;
        GG_ActivityDataMonitor_NotifyActivityChange(self, GG_System_GetCurrentTimestamp());
    }

    // schedule a timer to monitor inactivity
    if (self->inactivity_timer && self->inactivity_timeout) {
        GG_Timer_Schedule(self->inactivity_timer,
                          GG_CAST(self, GG_TimerListener),
                          self->inactivity_timeout);
    }

    return result;
}

//----------------------------------------------------------------------
static GG_Result
GG_ActivityDataMonitor_SetListener(GG_DataSink* _self, GG_DataSinkListener* listener)
{
    GG_ActivityDataMonitor* self = GG_SELF(GG_ActivityDataMonitor, GG_DataSink);

    // keep a pointer to the listener
    self->sink_listener = listener;

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
static void
GG_ActivityDataMonitor_OnCanPut(GG_DataSinkListener* _self)
{
    GG_ActivityDataMonitor* self = GG_SELF(GG_ActivityDataMonitor, GG_DataSinkListener);

    // notify the listener that they can put now
    if (self->sink_listener) {
        GG_DataSinkListener_OnCanPut(self->sink_listener);
    }
}

//----------------------------------------------------------------------
static void
GG_ActivityDataMonitor_OnTimerFired(GG_TimerListener* _self, GG_Timer* timer, uint32_t elapsed)
{
    GG_COMPILER_UNUSED(timer);
    GG_COMPILER_UNUSED(elapsed);

    GG_ActivityDataMonitor* self = GG_SELF(GG_ActivityDataMonitor, GG_TimerListener);

    GG_LOG_FINER("%s activity data stopped.",
        self->direction == GG_ACTIVITY_MONITOR_DIRECTION_BOTTOM_TO_TOP ? "Incoming" : "Outgoing");

    self->active = false;
    GG_Timestamp time = GG_System_GetCurrentTimestamp() - self->inactivity_timeout * GG_NANOSECONDS_PER_MILLISECOND;

    GG_ActivityDataMonitor_NotifyActivityChange(self, time);
}

#if defined(GG_CONFIG_ENABLE_INSPECTION)
//----------------------------------------------------------------------
GG_Inspectable*
GG_ActivityDataMonitor_AsInspectable(GG_ActivityDataMonitor* self)
{
    return GG_CAST(self, GG_Inspectable);
}

//----------------------------------------------------------------------
static GG_Result
GG_ActivityDataMonitor_Inspect(GG_Inspectable* _self, GG_Inspector* inspector, const GG_InspectionOptions* options)
{
    GG_COMPILER_UNUSED(options);
    GG_ActivityDataMonitor* self = GG_SELF(GG_ActivityDataMonitor, GG_Inspectable);

    GG_Inspector_OnBoolean(inspector, "active", self->active);

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(GG_ActivityDataMonitor, GG_Inspectable) {
    .Inspect = GG_ActivityDataMonitor_Inspect
};
#endif

//----------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(GG_ActivityDataMonitor, GG_DataSink) {
    GG_ActivityDataMonitor_PutData,
    GG_ActivityDataMonitor_SetListener
};

GG_IMPLEMENT_INTERFACE(GG_ActivityDataMonitor, GG_DataSource) {
    GG_ActivityDataMonitor_SetDataSink
};

GG_IMPLEMENT_INTERFACE(GG_ActivityDataMonitor, GG_DataSinkListener) {
    GG_ActivityDataMonitor_OnCanPut
};

GG_IMPLEMENT_INTERFACE(GG_ActivityDataMonitor, GG_TimerListener) {
    GG_ActivityDataMonitor_OnTimerFired
};

//----------------------------------------------------------------------
GG_Result
GG_ActivityDataMonitor_Create(GG_TimerScheduler*       scheduler,
                              uint32_t                 direction,
                              uint32_t                 inactivity_timeout,
                              GG_ActivityDataMonitor** source)
{
    GG_ASSERT(scheduler);
    GG_ASSERT(source);

    *source = NULL;

    // allocate a new object
    GG_ActivityDataMonitor* self = GG_AllocateZeroMemory(sizeof(GG_ActivityDataMonitor));
    if (self == NULL) return GG_ERROR_OUT_OF_MEMORY;

    // initialize the object
    GG_EventEmitterBase_Init(&self->event_emitter);
    self->inactivity_timeout = inactivity_timeout;
    self->direction          = direction;

    // Schedule a timer to detect when we should notify that we stopped receiving data
    GG_Result result = GG_TimerScheduler_CreateTimer(scheduler, &self->inactivity_timer);
    if (GG_FAILED(result)) {
        GG_ActivityDataMonitor_Destroy(self);
        return result;
    }

    // initialize the object interfaces
    GG_SET_INTERFACE(self, GG_ActivityDataMonitor, GG_DataSink);
    GG_SET_INTERFACE(self, GG_ActivityDataMonitor, GG_DataSource);
    GG_SET_INTERFACE(self, GG_ActivityDataMonitor, GG_DataSinkListener);
    GG_SET_INTERFACE(self, GG_ActivityDataMonitor, GG_TimerListener);
    GG_IF_INSPECTION_ENABLED(GG_SET_INTERFACE(self, GG_ActivityDataMonitor, GG_Inspectable));

    *source = self;
    return GG_SUCCESS;
}

//----------------------------------------------------------------------
void
GG_ActivityDataMonitor_Destroy(GG_ActivityDataMonitor* self)
{
    if (self == NULL) return;

    // de-register as a listener from the sink
    if (self->sink) {
        GG_DataSink_SetListener(self->sink, NULL);
    }

    if (self->inactivity_timer != NULL) {
        GG_Timer_Destroy(self->inactivity_timer);
    }

    // free the object memory
    GG_ClearAndFreeObject(self, 4);
}

//----------------------------------------------------------------------
GG_DataSink*
GG_ActivityDataMonitor_AsDataSink(GG_ActivityDataMonitor* self)
{
    return GG_CAST(self, GG_DataSink);
}

//----------------------------------------------------------------------
GG_DataSource*
GG_ActivityDataMonitor_AsDataSource(GG_ActivityDataMonitor* self)
{
    return GG_CAST(self, GG_DataSource);
}

//----------------------------------------------------------------------
GG_EventEmitter*
GG_ActivityDataMonitor_AsEventEmitter(GG_ActivityDataMonitor* self)
{
    return GG_CAST(&self->event_emitter, GG_EventEmitter);
}

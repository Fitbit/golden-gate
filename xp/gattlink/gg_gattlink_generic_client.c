/**
 * @file
 * @copyright
 * Copyright 2017-2020 Fitbit, Inc
 * SPDX-License-Identifier: Apache-2.0
 *
 * @date 2017-09-14
 * @details
 * Implementation of a generic Gattlink client.
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xp/common/gg_logging.h"
#include "xp/common/gg_utils.h"
#include "xp/common/gg_system.h"
#include "xp/gattlink/gg_gattlink.h"
#include "xp/gattlink/gg_gattlink_generic_client.h"
#include "xp/utils/gg_data_probe.h"

/*----------------------------------------------------------------------
|   logging
+---------------------------------------------------------------------*/
GG_SET_LOCAL_LOGGER("gg.xp.gattlink.generic-client")

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/

struct GG_GattlinkGenericClient {
    GG_IMPLEMENTS(GG_GattlinkClient);
    GG_IMPLEMENTS(GG_TimerListener);
    GG_IF_INSPECTION_ENABLED(GG_IMPLEMENTS(GG_Inspectable);)

    GG_EventEmitterBase     event_emitter;
    bool                    session_open;
    GG_GattlinkProtocol*    protocol;
    GG_FrameSerializer*     frame_serializer;
    GG_FrameAssembler*      frame_assembler;
    GG_RingBuffer           output_buffer;
    size_t                  max_transport_fragment_size;
    GG_DataProbe*           probe;
    GG_GattlinkProbeConfig  probe_config;
    bool                    buffer_over_threshold;
    GG_Timer*               buffer_fullness_timer;
    struct {
        GG_IMPLEMENTS(GG_DataSink);
        GG_IMPLEMENTS(GG_DataSource);
        GG_IMPLEMENTS(GG_DataSinkListener);

        GG_DataSink*         sink;
        GG_DataSinkListener* sink_listener;
    } transport_side;
    struct {
        GG_IMPLEMENTS(GG_DataSink);
        GG_IMPLEMENTS(GG_DataSource);
        GG_IMPLEMENTS(GG_DataSinkListener);

        GG_DataSink*         sink;
        GG_DataSinkListener* sink_listener;
    } user_side;
};

//----------------------------------------------------------------------
static void
GG_GattlinkGenericClient_NotifyBufferFullness(GG_GattlinkGenericClient* self,
                                              bool                      over_threshold)
{
    // notify the event listener if we have one
    if (self->event_emitter.listener) {
        GG_Event event = {
            .type   = over_threshold ?
                GG_GENERIC_GATTLINK_CLIENT_OUTPUT_BUFFER_OVER_THRESHOLD :
                GG_GENERIC_GATTLINK_CLIENT_OUTPUT_BUFFER_UNDER_THRESHOLD,
            .source = self
        };
        GG_EventListener_OnEvent(self->event_emitter.listener, &event);
    }
}

//----------------------------------------------------------------------
// Helper function to update data probe buffer usage. Will also generate a report from the probe,
// analyze the report, and post an event to indicate a state transition in the buffer usage if
// necessary.
static void
GG_GattlinkGenericClient_UpdateBufferState(GG_GattlinkGenericClient* self, bool force_event)
{
    GG_Result result;

    // accumulate latest bufferred amount
    GG_Timestamp now = GG_System_GetCurrentTimestamp();
    size_t bytes_buffered = GG_RingBuffer_GetAvailable(&self->output_buffer);
    GG_DataProbe_AccumulateWithTime(self->probe, bytes_buffered, now);

    // generate report
    const GG_DataProbeReport* report = GG_DataProbe_GetReportWithTime(self->probe, now);

    GG_LOG_FINE("Windowed byte-seconds in last %d ms: %d",
                (int)GG_DataProbe_GetWindowSize(self->probe),
                (int)report->window_bytes_second);

    bool buffer_over_threshold =
        report->window_bytes_second > self->probe_config.buffer_threshold;

    // emit an event immediately if buffer fullness changed
    if (buffer_over_threshold != self->buffer_over_threshold || force_event) {
        self->buffer_over_threshold = buffer_over_threshold;
        GG_GattlinkGenericClient_NotifyBufferFullness(self, buffer_over_threshold);
    }

    // start a timer to repeat sending event until buffer fullness falls under threshold
    if (self->buffer_over_threshold) {
        result = GG_Timer_Schedule(self->buffer_fullness_timer,
                                   GG_CAST(self, GG_TimerListener),
                                   GG_GENERIC_GATTLINK_CLIENT_OUTPUT_BUFFER_MONITOR_TIMEOUT);
        if (GG_FAILED(result)) {
            GG_LOG_WARNING("Failed to start Gattlink buffer fullness timer");
        }
    } else { // Unschedule timer after transition to under threshold
        GG_Timer_Unschedule(self->buffer_fullness_timer);
    }
}

//----------------------------------------------------------------------
static size_t
GG_GattlinkGenericClient_GetOutgoingDataAvailable(GG_GattlinkClient* _self)
{
    GG_GattlinkGenericClient* self = GG_SELF(GG_GattlinkGenericClient, GG_GattlinkClient);
    return GG_RingBuffer_GetAvailable(&self->output_buffer);
}

//----------------------------------------------------------------------
static GG_Result
GG_GattlinkGenericClient_GetOutgoingData(GG_GattlinkClient* _self, size_t offset, void* buffer, size_t buffer_size)
{
    GG_GattlinkGenericClient* self = GG_SELF(GG_GattlinkGenericClient, GG_GattlinkClient);
    GG_LOG_FINER("getting %u bytes @ %u from the ring buffer", (int)buffer_size, (int)offset);
    size_t bytes_peeked = GG_RingBuffer_Peek(&self->output_buffer, buffer, offset, buffer_size);
    if (bytes_peeked == buffer_size) {
        return GG_SUCCESS;
    } else {
        return GG_ERROR_OUT_OF_RANGE;
    }
}

//----------------------------------------------------------------------
static void
GG_GattlinkGenericClient_ConsumeOutgoingData(GG_GattlinkClient* _self, size_t bytes_consumed)
{
    GG_GattlinkGenericClient* self = GG_SELF(GG_GattlinkGenericClient, GG_GattlinkClient);
    if (GG_RingBuffer_GetAvailable(&self->output_buffer) >= bytes_consumed) {
        GG_LOG_FINE("%d bytes consumed", (int)bytes_consumed);
        GG_RingBuffer_MoveOut(&self->output_buffer, bytes_consumed);

        // notify listeners who may be waiting for some space to free up
        if (self->user_side.sink_listener) {
            GG_DataSinkListener_OnCanPut(self->user_side.sink_listener);
        }
    } else {
        GG_LOG_WARNING("unexpected value offset=%u, exceeds ring buffer fullness", (int)bytes_consumed);
    }

    // update data probe if configured
    if (self->probe != NULL) {
        GG_GattlinkGenericClient_UpdateBufferState(self, false);
    }
}

//----------------------------------------------------------------------
static void
GG_GattlinkGenericClient_NotifyIncomingDataAvailable(GG_GattlinkClient* _self)
{
    GG_GattlinkGenericClient* self = GG_SELF(GG_GattlinkGenericClient, GG_GattlinkClient);

    // feed the available data to the frame assembler until it stops consuming it
    for (; self->user_side.sink != NULL;) {
        // first, see how much we can feed the frame assembler
        uint8_t* feed_buffer = NULL;
        size_t   feed_buffer_size = 0;
        GG_FrameAssembler_GetFeedBuffer(self->frame_assembler, &feed_buffer, &feed_buffer_size);
        GG_LOG_FINEST("feed_buffer_size = %u", (int)feed_buffer_size);

        // stop now if the assembler doesn't want any data at this point
        if (feed_buffer_size == 0) {
            break;
        }

        // check what's available
        size_t bytes_available = GG_GattlinkProtocol_GetIncomingDataAvailable(self->protocol);
        GG_LOG_FINER("%u bytes of data available", (int)bytes_available);

        // only feed up to what's available
        if (feed_buffer_size > bytes_available) {
            feed_buffer_size = bytes_available;
        }

        // feed the frame assembler by copying into its feed buffer and then letting it know
        // how much we wrote
        int gl_result = GG_GattlinkProtocol_GetIncomingData(self->protocol,
                                                            0,
                                                            feed_buffer,
                                                            feed_buffer_size);
        if (gl_result < 0) {
            GG_LOG_WARNING("GG_GattlinkProtocol_GetIncomingData failed (%d)", gl_result);
            return;
        }
        GG_Buffer* frame = NULL;
        GG_Result result = GG_FrameAssembler_Feed(self->frame_assembler, &feed_buffer_size, &frame);
        if (GG_FAILED(result)) {
            GG_LOG_WARNING("GG_FrameAssembler_Feed failed (%d)", result);
        }

        // if we got a frame, send it now
        if (frame) {
            GG_LOG_FINE("got a frame");
            result = GG_DataSink_PutData(self->user_side.sink, frame, NULL);
            GG_Buffer_Release(frame);
            if (GG_FAILED(result)) {
                // couldn't send, so don't consume this data, we'll feed it again later
                GG_LOG_WARNING("GG_DataSink_PutData failed (%d)", result);
                break;
            }
        }

        // let the protocol know how much data we consumed
        if (feed_buffer_size) {
            GG_GattlinkProtocol_ConsumeIncomingData(self->protocol, feed_buffer_size);
        } else {
            // nothing consumed?
            GG_LOG_WARNING("no data consumed by the frame assembler?");
            break;
        }

        // stop if we've fed everything
        if (feed_buffer_size == bytes_available) {
            break;
        }
    }
}

//----------------------------------------------------------------------
static size_t
GG_GattlinkGenericClient_GetTransportMaxPacketSize(GG_GattlinkClient* _self)
{
    GG_GattlinkGenericClient* self = GG_SELF(GG_GattlinkGenericClient, GG_GattlinkClient);
    return GG_MIN(self->max_transport_fragment_size, GG_GATTLINK_MAX_PACKET_SIZE);
}

//----------------------------------------------------------------------
static GG_Result
GG_GattlinkGenericClient_SendRawData(GG_GattlinkClient* _self, const void* data, size_t data_size)
{
    GG_GattlinkGenericClient* self = GG_SELF(GG_GattlinkGenericClient, GG_GattlinkClient);
    // check that we have a tansport sink
    if (self->transport_side.sink == NULL) {
        return GG_ERROR_INVALID_STATE;
    }

    // allocate a buffer to wrap the data
    GG_DynamicBuffer* buffer;
    GG_Result result = GG_DynamicBuffer_Create(data_size, &buffer);
    if (GG_FAILED(result)) return result;

    // copy the data into the buffer
    GG_DynamicBuffer_SetData(buffer, data, data_size);

    // send the buffer to the transport-side sink, ignoring errors
    GG_LOG_FINE("sending %u bytes to the transport", (int)data_size);
    GG_DataSink_PutData(self->transport_side.sink,
                        GG_DynamicBuffer_AsBuffer(buffer),
                        NULL);

    // don't keep a reference to the buffer
    GG_DynamicBuffer_Release(buffer);

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
static void
GG_GattlinkGenericClient_Flush(GG_GattlinkGenericClient* self)
{
    // reset the frame assembler
    if (self->frame_assembler) {
        GG_FrameAssembler_Reset(self->frame_assembler);
    }

    // reset the ring buffer
    GG_RingBuffer_Reset(&self->output_buffer);
}

//----------------------------------------------------------------------
static void
GG_GattlinkGenericClient_NotifySessionReady(GG_GattlinkClient* _self)
{
    GG_GattlinkGenericClient* self = GG_SELF(GG_GattlinkGenericClient, GG_GattlinkClient);

    // remember that we have an open session
    self->session_open = true;

    // notify any listener who may be waiting for this
    if (self->user_side.sink_listener) {
        GG_DataSinkListener_OnCanPut(self->user_side.sink_listener);
    }

    // notify the event listener if we have one
    if (self->event_emitter.listener) {
        GG_Event event = {
            .type   = GG_EVENT_TYPE_GATTLINK_SESSION_READY,
            .source = self
        };
        GG_EventListener_OnEvent(self->event_emitter.listener, &event);
    }
}

//----------------------------------------------------------------------
static void
GG_GattlinkGenericClient_NotifySessionReset(GG_GattlinkClient* _self)
{
    GG_GattlinkGenericClient* self = GG_SELF(GG_GattlinkGenericClient, GG_GattlinkClient);

    // flush the buffers
    GG_GattlinkGenericClient_Flush(self);

    // the session will be closed until we get notified that it is ready again
    self->session_open = false;

    // notify the event listener if we have one
    if (self->event_emitter.listener) {
        GG_Event event = {
            .type   = GG_EVENT_TYPE_GATTLINK_SESSION_RESET,
            .source = self
        };
        GG_EventListener_OnEvent(self->event_emitter.listener, &event);
    }
}

//----------------------------------------------------------------------
static void
GG_GattlinkGenericClient_NotifySessionStalled(GG_GattlinkClient* _self, uint32_t stalled_time)
{
    GG_GattlinkGenericClient* self = GG_SELF(GG_GattlinkGenericClient, GG_GattlinkClient);

    // notify the event listener if we have one
    if (self->event_emitter.listener) {
        GG_GattlinkStalledEvent event = {
            .base = {
                .type   = GG_EVENT_TYPE_GATTLINK_SESSION_STALLED,
                .source = self
            },
            .stalled_time = stalled_time
        };

        GG_EventListener_OnEvent(self->event_emitter.listener, &event.base);
    }
}

//----------------------------------------------------------------------
static GG_Result
GG_GattlinkGenericClient_UserSide_PutData(GG_DataSink* _self, GG_Buffer* data, const GG_BufferMetadata* metadata)
{
    GG_COMPILER_UNUSED(metadata);
    GG_GattlinkGenericClient* self = GG_SELF_M(user_side, GG_GattlinkGenericClient, GG_DataSink);

    // wait until the session is open
    if (!self->session_open) {
        return GG_ERROR_WOULD_BLOCK;
    }

    // check if we have enough space in the ring buffer
    size_t space_available = GG_RingBuffer_GetSpace(&self->output_buffer);
    size_t data_size = GG_Buffer_GetDataSize(data);
    GG_LOG_FINE("space available in ring buffer = %u, need to write %u bytes",
                (int)space_available, (int)data_size);
    if (data_size > space_available) {
        return GG_ERROR_WOULD_BLOCK;
    }

    // serialize the frame
    GG_Result result = GG_FrameSerializer_SerializeFrame(self->frame_serializer,
                                                         GG_Buffer_GetData(data),
                                                         data_size,
                                                         &self->output_buffer);
    if (GG_FAILED(result)) {
        return result;
    }

    // update the probe if configured
    if (self->probe) {
        GG_GattlinkGenericClient_UpdateBufferState(self, false);
    }

    // notify the protocol handler that new data has arrived
    GG_GattlinkProtocol_NotifyOutgoingDataAvailable(self->protocol);

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
static GG_Result
GG_GattlinkGenericClient_UserSide_SetListener(GG_DataSink* _self, GG_DataSinkListener* listener)
{
    GG_GattlinkGenericClient* self = GG_SELF_M(user_side, GG_GattlinkGenericClient, GG_DataSink);

    self->user_side.sink_listener = listener;

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(GG_GattlinkGenericClient_UserSide, GG_DataSink) {
    GG_GattlinkGenericClient_UserSide_PutData,
    GG_GattlinkGenericClient_UserSide_SetListener
};

//----------------------------------------------------------------------
static GG_Result
GG_GattlinkGenericClient_UserSide_SetDataSink(GG_DataSource* _self, GG_DataSink* sink)
{
    GG_GattlinkGenericClient* self = GG_SELF_M(user_side, GG_GattlinkGenericClient, GG_DataSource);

    // de-register as a listener from the current sink
    if (self->user_side.sink) {
        GG_DataSink_SetListener(self->user_side.sink, NULL);
    }

    // keep a reference to the new sink
    self->user_side.sink = sink;

    // register as a listener
    if (sink) {
        GG_DataSink_SetListener(sink, GG_CAST(&self->user_side, GG_DataSinkListener));
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(GG_GattlinkGenericClient_UserSide, GG_DataSource) {
    GG_GattlinkGenericClient_UserSide_SetDataSink
};


//----------------------------------------------------------------------
static void
GG_GattlinkGenericClient_UserSide_OnCanPut(GG_DataSinkListener* _self)
{
    GG_GattlinkGenericClient* self = GG_SELF_M(user_side, GG_GattlinkGenericClient, GG_DataSinkListener);

    // try to send process data if any is available
    GG_GattlinkGenericClient_NotifyIncomingDataAvailable(GG_CAST(self, GG_GattlinkClient));
}

//----------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(GG_GattlinkGenericClient_UserSide, GG_DataSinkListener) {
    GG_GattlinkGenericClient_UserSide_OnCanPut
};

//----------------------------------------------------------------------
static GG_Result
GG_GattlinkGenericClient_TransportSide_PutData(GG_DataSink* _self, GG_Buffer* data, const GG_BufferMetadata* metadata)
{
    GG_COMPILER_UNUSED(metadata);
    GG_GattlinkGenericClient* self = GG_SELF_M(transport_side, GG_GattlinkGenericClient, GG_DataSink);

    GG_LOG_FINE("transport data, size=%u", (int)GG_Buffer_GetDataSize(data));

    // forward the data to the protocol handler (ignore errors)
    GG_Result result = GG_GattlinkProtocol_HandleIncomingRawData(self->protocol,
                                                                 GG_Buffer_GetData(data),
                                                                 GG_Buffer_GetDataSize(data));
    if (GG_FAILED(result)) {
        GG_LOG_WARNING("GG_GattlinkProtocol_HandleIncomingRawData failed (%d)", result);
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
static GG_Result
GG_GattlinkGenericClient_TransportSide_SetListener(GG_DataSink* _self, GG_DataSinkListener* listener)
{
    GG_COMPILER_UNUSED(_self);
    GG_COMPILER_UNUSED(listener);
    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(GG_GattlinkGenericClient_TransportSide, GG_DataSink) {
    GG_GattlinkGenericClient_TransportSide_PutData,
    GG_GattlinkGenericClient_TransportSide_SetListener
};

//----------------------------------------------------------------------
static GG_Result
GG_GattlinkGenericClient_TransportSide_SetDataSink(GG_DataSource* _self, GG_DataSink* sink)
{
    GG_GattlinkGenericClient* self = GG_SELF_M(transport_side, GG_GattlinkGenericClient, GG_DataSource);

    // de-register as a listener from the current sink
    if (self->transport_side.sink) {
        GG_DataSink_SetListener(self->transport_side.sink, NULL);
    }

    // keep a reference to the new sink
    self->transport_side.sink = sink;

    // register as a listener
    if (sink) {
        GG_DataSink_SetListener(sink, GG_CAST(&self->transport_side, GG_DataSinkListener));
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(GG_GattlinkGenericClient_TransportSide, GG_DataSource) {
    GG_GattlinkGenericClient_TransportSide_SetDataSink
};

//----------------------------------------------------------------------
static void
GG_GattlinkGenericClient_TransportSide_OnCanPut(GG_DataSinkListener* _self)
{
    GG_GattlinkGenericClient* self = GG_SELF_M(transport_side, GG_GattlinkGenericClient, GG_DataSinkListener);

    // check if there's something to send
    GG_GattlinkProtocol_NotifyOutgoingDataAvailable(self->protocol);
}

//----------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(GG_GattlinkGenericClient_TransportSide, GG_DataSinkListener) {
    GG_GattlinkGenericClient_TransportSide_OnCanPut
};

//----------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(GG_GattlinkGenericClient, GG_GattlinkClient) {
    GG_GattlinkGenericClient_GetOutgoingDataAvailable,
    GG_GattlinkGenericClient_GetOutgoingData,
    GG_GattlinkGenericClient_ConsumeOutgoingData,
    GG_GattlinkGenericClient_NotifyIncomingDataAvailable,
    GG_GattlinkGenericClient_GetTransportMaxPacketSize,
    GG_GattlinkGenericClient_SendRawData,
    GG_GattlinkGenericClient_NotifySessionReady,
    GG_GattlinkGenericClient_NotifySessionReset,
    GG_GattlinkGenericClient_NotifySessionStalled
};

#if defined(GG_CONFIG_ENABLE_INSPECTION)
//----------------------------------------------------------------------
static GG_Result
GG_GattlinkGenericClient_Inspect(GG_Inspectable* _self, GG_Inspector* inspector, const GG_InspectionOptions* options)
{
    GG_COMPILER_UNUSED(options);
    GG_GattlinkGenericClient* self = GG_SELF(GG_GattlinkGenericClient, GG_Inspectable);

    GG_Inspector_OnBoolean(inspector, "session_open", self->session_open);
    GG_Inspector_OnInteger(inspector,
                           "ring_buffer_size",
                           self->output_buffer.size,
                           GG_INSPECTOR_FORMAT_HINT_UNSIGNED);
    GG_Inspector_OnInteger(inspector,
                           "ring_buffer_fullness",
                           GG_RingBuffer_GetAvailable(&self->output_buffer),
                           GG_INSPECTOR_FORMAT_HINT_UNSIGNED);
    GG_Inspector_OnInteger(inspector,
                           "max_transport_fragment_size",
                           self->max_transport_fragment_size,
                           GG_INSPECTOR_FORMAT_HINT_UNSIGNED);

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(GG_GattlinkGenericClient, GG_Inspectable) {
    .Inspect = GG_GattlinkGenericClient_Inspect
};
#endif

//----------------------------------------------------------------------
//  Called when the buffer fullness update timer has fired
//----------------------------------------------------------------------
static void
GG_GattlinkGenericClient_OnTimerFired(GG_TimerListener* _self, GG_Timer* timer, uint32_t elapsed)
{
    GG_GattlinkGenericClient* self = GG_SELF(GG_GattlinkGenericClient, GG_TimerListener);
    GG_COMPILER_UNUSED(timer);
    GG_COMPILER_UNUSED(elapsed);

    // update probe
    if (self->probe != NULL) {
        GG_GattlinkGenericClient_UpdateBufferState(self, true);
    }
}

//----------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(GG_GattlinkGenericClient, GG_TimerListener) {
    .OnTimerFired = GG_GattlinkGenericClient_OnTimerFired
};

//----------------------------------------------------------------------
GG_Result
GG_GattlinkGenericClient_Create(GG_TimerScheduler*            timer_scheduler,
                                size_t                        buffer_size,
                                uint8_t                       max_tx_window_size,
                                uint8_t                       max_rx_window_size,
                                size_t                        max_transport_fragment_size,
                                const GG_GattlinkProbeConfig* probe_config,
                                GG_FrameSerializer*           frame_serializer,
                                GG_FrameAssembler*            frame_assembler,
                                GG_GattlinkGenericClient**    generic_client)
{
    GG_ASSERT(frame_assembler);

    *generic_client = NULL;

    GG_GattlinkGenericClient *self = (GG_GattlinkGenericClient*)GG_AllocateZeroMemory(sizeof(GG_GattlinkGenericClient));
    if (self == NULL) {
        return GG_ERROR_OUT_OF_MEMORY;
    }

    uint8_t *buffer = (uint8_t *)GG_AllocateMemory(buffer_size);
    if (buffer == NULL) {
        GG_FreeMemory(self);
        return GG_ERROR_OUT_OF_MEMORY;
    }

    // check window bounds and compute final values
    GG_GattlinkSessionConfig config = {
        max_tx_window_size ? max_tx_window_size : GG_GENERIC_GATTLINK_CLIENT_DEFAULT_MAX_TX_WINDOW_SIZE,
        max_rx_window_size ? max_rx_window_size : GG_GENERIC_GATTLINK_CLIENT_DEFAULT_MAX_RX_WINDOW_SIZE
    };

    // setup the state
    GG_EventEmitterBase_Init(&self->event_emitter);
    self->session_open                = false;
    self->max_transport_fragment_size = max_transport_fragment_size;
    self->frame_serializer            = frame_serializer;
    self->frame_assembler             = frame_assembler;
    self->protocol                    = NULL;

    // set the interfaces
    GG_SET_INTERFACE(self,                  GG_GattlinkGenericClient,               GG_GattlinkClient);
    GG_SET_INTERFACE(self,                  GG_GattlinkGenericClient,               GG_TimerListener);
    GG_SET_INTERFACE(&self->user_side,      GG_GattlinkGenericClient_UserSide,      GG_DataSink);
    GG_SET_INTERFACE(&self->user_side,      GG_GattlinkGenericClient_UserSide,      GG_DataSource);
    GG_SET_INTERFACE(&self->user_side,      GG_GattlinkGenericClient_UserSide,      GG_DataSinkListener);
    GG_SET_INTERFACE(&self->transport_side, GG_GattlinkGenericClient_TransportSide, GG_DataSink);
    GG_SET_INTERFACE(&self->transport_side, GG_GattlinkGenericClient_TransportSide, GG_DataSource);
    GG_SET_INTERFACE(&self->transport_side, GG_GattlinkGenericClient_TransportSide, GG_DataSinkListener);
    GG_IF_INSPECTION_ENABLED(GG_SET_INTERFACE(self, GG_GattlinkGenericClient, GG_Inspectable));

    // init the buffer
    GG_RingBuffer_Init(&self->output_buffer, buffer, buffer_size);

    // register the client
    GG_GattlinkClient* client = GG_CAST(self, GG_GattlinkClient);
    GG_Result result = GG_GattlinkProtocol_Create(client,
                                                  &config,
                                                  timer_scheduler,
                                                  &self->protocol);
    if (GG_FAILED(result)) {
        GG_GattlinkGenericClient_Destroy(self);
        return result;
    }

    // instantiate probe if a config is passed
    if (probe_config != NULL) {
        memcpy(&self->probe_config, probe_config, sizeof(self->probe_config));
        result = GG_DataProbe_Create(GG_DATA_PROBE_OPTION_WINDOW_INTEGRAL,
                                     self->probe_config.buffer_sample_count,
                                     self->probe_config.window_size_ms,
                                     0,
                                     NULL,
                                     &self->probe);
        if (GG_FAILED(result)) {
            GG_LOG_WARNING("Unable to create data probe!");
            goto end;
        }

        result = GG_TimerScheduler_CreateTimer(timer_scheduler, &self->buffer_fullness_timer);
        if (GG_FAILED(result)) {
            GG_LOG_WARNING("Unable to create data probe!");
            goto end;
        }

        self->buffer_over_threshold = false;
    }

    *generic_client = self;

    return GG_SUCCESS;

end:
    GG_GattlinkGenericClient_Destroy(self);
    return result;
}

//----------------------------------------------------------------------
void
GG_GattlinkGenericClient_Destroy(GG_GattlinkGenericClient* self)
{
    if (self == NULL) return;

    // de-register as a listener from the current sinks
    if (self->user_side.sink) {
        GG_DataSink_SetListener(self->user_side.sink, NULL);
    }
    if (self->transport_side.sink) {
        GG_DataSink_SetListener(self->transport_side.sink, NULL);
    }

    // free the ring buffer memory
    GG_FreeMemory(self->output_buffer.data.start);

    // deregister the client
    GG_GattlinkProtocol_Destroy(self->protocol);

    // reset references
    self->transport_side.sink = NULL;
    self->transport_side.sink_listener = NULL;
    self->user_side.sink = NULL;
    self->user_side.sink_listener = NULL;

    // free the probe and timer
    GG_DataProbe_Destroy(self->probe);
    GG_Timer_Destroy(self->buffer_fullness_timer);

    // free the object memory
    GG_ClearAndFreeObject(self, 2);
}

//----------------------------------------------------------------------
GG_EventEmitter*
GG_GattlinkGenericClient_AsEventEmitter(GG_GattlinkGenericClient* self)
{
    return GG_CAST(&self->event_emitter, GG_EventEmitter);
}

#if defined(GG_CONFIG_ENABLE_INSPECTION)
//----------------------------------------------------------------------
GG_Inspectable*
GG_GattlinkGenericClient_AsInspectable(GG_GattlinkGenericClient* self)
{
    return GG_CAST(self, GG_Inspectable);
}
#endif

//----------------------------------------------------------------------
GG_Result
GG_GattlinkGenericClient_Start(GG_GattlinkGenericClient* self)
{
    return GG_GattlinkProtocol_Start(self->protocol);
}

//----------------------------------------------------------------------
GG_Result
GG_GattlinkGenericClient_Reset(GG_GattlinkGenericClient* self)
{
    // flush buffers
    GG_GattlinkGenericClient_Flush(self);

    // reset the protocol
    GG_GattlinkProtocol_Reset(self->protocol);

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_Result
GG_GattlinkGenericClient_SetMaxTransportFragmentSize(GG_GattlinkGenericClient* self,
                                                     size_t                    max_transport_fragment_size)
{
    self->max_transport_fragment_size = max_transport_fragment_size;

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_DataSink*
GG_GattlinkGenericClient_GetUserSideAsDataSink(GG_GattlinkGenericClient* self)
{
    return GG_CAST(&self->user_side, GG_DataSink);
}

//----------------------------------------------------------------------
GG_DataSource*
GG_GattlinkGenericClient_GetUserSideAsDataSource(GG_GattlinkGenericClient* self)
{
    return GG_CAST(&self->user_side, GG_DataSource);
}

//----------------------------------------------------------------------
GG_DataSink*
GG_GattlinkGenericClient_GetTransportSideAsDataSink(GG_GattlinkGenericClient* self)
{
    return GG_CAST(&self->transport_side, GG_DataSink);
}

//----------------------------------------------------------------------
GG_DataSource*
GG_GattlinkGenericClient_GetTransportSideAsDataSource(GG_GattlinkGenericClient* self)
{
    return GG_CAST(&self->transport_side, GG_DataSource);
}

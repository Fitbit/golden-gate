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
 * @date 2018-12-06
 *
 * @details
 * CoAP Event Emitter
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include "xp/common/gg_logging.h"
#include "xp/common/gg_threads.h"
#include "xp/coap/gg_coap.h"
#include "gg_coap_event_emitter.h"

/*----------------------------------------------------------------------
|   logging
+---------------------------------------------------------------------*/
GG_SET_LOCAL_LOGGER("gg.xp.utils.coap-event-emitter")

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
typedef struct {
    uint32_t event_type;   // event type, or 0 when not set
    uint32_t window_end;   // in ms, relative to the scheduler's origin
    bool     in_flight;    // true if part of the current outgoing set
} GG_CoapEventEmitterEntry;

struct GG_CoapEventEmitter {
    GG_IMPLEMENTS(GG_TimerListener);
    GG_IMPLEMENTS(GG_CoapResponseListener);
    GG_IMPLEMENTS(GG_BufferSource); // to generate the request payload on the fly

    GG_THREAD_GUARD_ENABLE_BINDING

    GG_CoapEndpoint*           coap_endpoint;           // endpoint to send requests through
    GG_CoapMessageOptionParam* coap_path_options;       // path fragments in option form
    size_t                     coap_path_options_count; // number of path fragments
    GG_CoapRequestHandle       coap_request_handle;     // last request handle
    uint32_t                   coap_request_timestamp;  // when we sent our last request
    GG_TimerScheduler*         timer_scheduler;         // scheduler to create/schedule timers
    GG_Timer*                  timer;                   // scheduling timer
    uint32_t                   retry_delay;             // retry delay for failed requests
    uint32_t                   min_request_age;         // min age of a request below which we won't cancel
    size_t                     entry_count;             // number of entries in the array that follows
    GG_CoapEventEmitterEntry   entries[];               // event entries (must be the last field)
};

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/
#define GG_COAP_EVENT_EMITTER_TYPE_NONE         0
#define GG_COAP_EVENT_EMITTER_NO_PENDING_EVENT  UINT32_MAX

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

//----------------------------------------------------------------------
static void
GG_CoapEventEmitter_GetRequestData(const GG_BufferSource* _self, void* data)
{
    const GG_CoapEventEmitter* self = GG_SELF(GG_CoapEventEmitter, GG_BufferSource);

    // Encode the message into the payload buffer
    // Here we don't use a library to do the encoding, so as not to have a hard
    // dependency on a schema file and protobuf encoding runtime. We can afford
    // to do this because the message format is very simple.
    uint8_t* protobuf = data;
    for (size_t i = 0; i < self->entry_count; i++) {
        if (self->entries[i].in_flight) {
            *protobuf++ = (1 << 3); // field_number = 1, wire_type = 0 (varint)
            size_t varint_size = GG_EncodeProtobufVarint(self->entries[i].event_type, protobuf);
            GG_ASSERT(varint_size);
            protobuf += varint_size;
        }
    }
}

//----------------------------------------------------------------------
static size_t
GG_CoapEventEmitter_GetRequestDataSize(const GG_BufferSource* _self)
{
    const GG_CoapEventEmitter* self = GG_SELF(GG_CoapEventEmitter, GG_BufferSource);

    // Compute the protobuf-encoded payload size.
    // Each event is a varint field entry: 1 byte for the field number and type, plus a varint value
    size_t protobuf_size = 0;
    for (size_t i = 0; i < self->entry_count; i++) {
        if (self->entries[i].in_flight) {
            protobuf_size += 1 + GG_ProtobufVarintSize(self->entries[i].event_type);
        }
    }

    return protobuf_size;
}

//----------------------------------------------------------------------
static GG_Result
GG_CoapEventEmitter_Emit(GG_CoapEventEmitter* self)
{
    // mark all current events as 'in flight'
    for (size_t i = 0; i < self->entry_count; i++) {
        if (self->entries[i].event_type != GG_COAP_EVENT_EMITTER_TYPE_NONE) {
            self->entries[i].in_flight = true;
        }
    }

    // send the request
    GG_Result result;
    result = GG_CoapEndpoint_SendRequestFromBufferSource(self->coap_endpoint,
                                                         GG_COAP_METHOD_POST,
                                                         self->coap_path_options,
                                                         (unsigned int)self->coap_path_options_count,
                                                         GG_CAST(self, GG_BufferSource),
                                                         NULL,
                                                         GG_CAST(self, GG_CoapResponseListener),
                                                         &self->coap_request_handle);

    if (GG_FAILED(result)) {
        GG_LOG_WARNING("GG_CoapEndpoint_SendRequest failed (%d), will retry later", result);
        GG_Timer_Schedule(self->timer, GG_CAST(self, GG_TimerListener), (uint32_t)self->retry_delay);
    } else {
        // remember when we sent the request
        self->coap_request_timestamp = GG_TimerScheduler_GetTime(self->timer_scheduler);

        // if we had a timer scheduled, that's not needed anymore since we're already waiting for a response
        if (GG_Timer_IsScheduled(self->timer)) {
            GG_Timer_Unschedule(self->timer);
        }
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
// Compute the next event window end.
// Returns GG_COAP_EVENT_EMITTER_NO_PENDING_EVENT if there is no pending event
//----------------------------------------------------------------------
static uint32_t
GG_CoapEventEmitter_GetNextWindowEnd(GG_CoapEventEmitter* self)
{
    uint32_t result = GG_COAP_EVENT_EMITTER_NO_PENDING_EVENT;
    for (size_t i = 0; i < self->entry_count; i++) {
        if (self->entries[i].event_type == GG_COAP_EVENT_EMITTER_TYPE_NONE) {
            continue;
        }
        if (self->entries[i].window_end < result) {
            result = self->entries[i].window_end;
        }
    }

    return result;
}

//----------------------------------------------------------------------
// Check the pending events and CoAP state and update the timer and/or
// CoAP request accordingly
//----------------------------------------------------------------------
static void
GG_CoapEventEmitter_Update(GG_CoapEventEmitter* self)
{
    // if we have a request in flight that is too old, cancel it
    uint32_t now = GG_TimerScheduler_GetTime(self->timer_scheduler);
    if (self->coap_request_handle && now > self->coap_request_timestamp) {
        uint32_t request_age = now - self->coap_request_timestamp;
        if (request_age > self->min_request_age) {
            GG_LOG_FINE("in-flight request is old enough to be canceled");
            GG_CoapEndpoint_CancelRequest(self->coap_endpoint, self->coap_request_handle);
            self->coap_request_handle    = 0;
            self->coap_request_timestamp = 0;
        }
    }

    // if we still have a request in flight, we'll just wait
    if (self->coap_request_handle) {
        GG_LOG_FINE("request still in flight");
        return;
    }

    // check if any event is ready to emit now
    uint32_t next_window_end = GG_CoapEventEmitter_GetNextWindowEnd(self);
    if (next_window_end != GG_COAP_EVENT_EMITTER_NO_PENDING_EVENT) {
        if (next_window_end <= now) {
            // the next window end is in the past or now, emit right away
            GG_LOG_FINE("emitting now");
            GG_CoapEventEmitter_Emit(self);
        } else {
            // (re)schedule a timer for the next window end
            GG_LOG_FINE("scheduling timer for %u ms from now", (int)next_window_end);
            GG_Timer_Schedule(self->timer, GG_CAST(self, GG_TimerListener), next_window_end - now);
        }
    }
}

//----------------------------------------------------------------------
static void
GG_CoapEventEmitter_OnTimerFired(GG_TimerListener* _self, GG_Timer* timer, uint32_t time_elapsed)
{
    GG_CoapEventEmitter* self = GG_SELF(GG_CoapEventEmitter, GG_TimerListener);
    GG_COMPILER_UNUSED(timer);
    GG_COMPILER_UNUSED(time_elapsed);

    GG_LOG_FINE("timer fired");

    // just check if there's anything to do
    GG_CoapEventEmitter_Update(self);
}

//----------------------------------------------------------------------
static void
GG_CoapEventEmitter_OnAck(GG_CoapResponseListener* self)
{
    GG_COMPILER_UNUSED(self);
}

//----------------------------------------------------------------------
static void
GG_CoapEventEmitter_OnError(GG_CoapResponseListener* _self, GG_Result error, const char* message)
{
    GG_CoapEventEmitter* self = GG_SELF(GG_CoapEventEmitter, GG_CoapResponseListener);
    GG_COMPILER_UNUSED(message);

    // retry
    GG_LOG_FINE("received error %d, will retry", error);
    self->coap_request_handle    = 0;
    self->coap_request_timestamp = 0;
    if (error == GG_ERROR_TIMEOUT) {
        // after a timeout, we can retry immediately
        GG_CoapEventEmitter_Update(self);
    } else {
        // after another error, wait before retrying
        GG_Timer_Schedule(self->timer, GG_CAST(self, GG_TimerListener), self->retry_delay);
    }
}

//----------------------------------------------------------------------
static void
GG_CoapEventEmitter_ClearEmittedEvents(GG_CoapEventEmitter* self) {
    for (size_t i = 0; i < self->entry_count; i++) {
        if (self->entries[i].in_flight) {
            // clear this entry
#if defined(GG_CONFIG_ENABLE_LOGGING)
            uint8_t type_chars[5] = { 0 };
            GG_BytesFromInt32Be(type_chars, self->entries[i].event_type);
            GG_LOG_FINER("clearing %s", type_chars);
#endif
            self->entries[i].event_type   = GG_COAP_EVENT_EMITTER_TYPE_NONE;
            self->entries[i].in_flight    = false;
            self->entries[i].window_end   = 0;
        }
    }
}

//----------------------------------------------------------------------
static void
GG_CoapEventEmitter_OnResponse(GG_CoapResponseListener* _self, GG_CoapMessage* response)
{
    GG_CoapEventEmitter* self = GG_SELF(GG_CoapEventEmitter, GG_CoapResponseListener);

    // the server received our emission, check the response
    GG_LOG_FINE("received response");
    self->coap_request_handle    = 0;
    self->coap_request_timestamp = 0;
    if (GG_CoapMessage_GetCode(response) == GG_COAP_MESSAGE_CODE_CHANGED) {
        // clear events that have already been emitted
        GG_CoapEventEmitter_ClearEmittedEvents(self);
    } else {
        uint8_t response_code = GG_CoapMessage_GetCode(response);
        GG_LOG_WARNING("received unexpected response: code=%d", (int)response_code);

        // check if we should continue trying or not
        if (GG_COAP_MESSAGE_CODE_CLASS(response_code) == GG_COAP_MESSAGE_CODE_CLASS_CLIENT_ERROR_RESPONSE) {
            // this is a client error, we won't retry
            GG_LOG_SEVERE("client error, will not retry");
            GG_CoapEventEmitter_ClearEmittedEvents(self);
        } else {
            // other type of response, retry later
            GG_Timer_Schedule(self->timer, GG_CAST(self, GG_TimerListener), self->retry_delay);
            return;
        }
    }

    // check if there's anything still pending
    GG_CoapEventEmitter_Update(self);
}

//----------------------------------------------------------------------
GG_Result
GG_CoapEventEmitter_Create(GG_CoapEndpoint*      coap_endpoint,
                           const char*           coap_path,
                           GG_TimerScheduler*    timer_scheduler,
                           size_t                max_events,
                           uint32_t              retry_delay,
                           uint32_t              min_request_age,
                           GG_CoapEventEmitter** emitter)
{
    GG_ASSERT(coap_endpoint);
    GG_ASSERT(coap_path);
    GG_ASSERT(timer_scheduler);
    GG_ASSERT(emitter);

    // allocate memory for the object
    size_t object_size = sizeof(GG_CoapEventEmitter) + max_events * sizeof(GG_CoapEventEmitterEntry);
    *emitter = GG_AllocateZeroMemory(object_size);
    if (*emitter == NULL) {
        return GG_ERROR_OUT_OF_MEMORY;
    }

    // init the object
    (*emitter)->coap_endpoint   = coap_endpoint;
    (*emitter)->entry_count     = max_events;
    (*emitter)->timer_scheduler = timer_scheduler;
    (*emitter)->retry_delay     = retry_delay ? retry_delay : GG_COAP_EVENT_EMITTER_DEFAULT_RETRY_DELAY;
    (*emitter)->min_request_age = min_request_age ? min_request_age : GG_COAP_EVENT_EMITTER_DEFAULT_MIN_REQUEST_AGE;

    // create a timer
    GG_Result result = GG_TimerScheduler_CreateTimer(timer_scheduler, &(*emitter)->timer);
    if (GG_FAILED(result)) {
        GG_FreeMemory(*emitter);
        return result;
    }

    // split the CoAP path into path segments
    GG_CoapMessageOptionParam coap_path_options[GG_COAP_EVENT_EMITTER_MAX_PATH_SEGMENTS];
    (*emitter)->coap_path_options_count = GG_COAP_EVENT_EMITTER_MAX_PATH_SEGMENTS;
    result = GG_Coap_SplitPathOrQuery(coap_path,
                                      '/',
                                      coap_path_options,
                                      &(*emitter)->coap_path_options_count,
                                      GG_COAP_MESSAGE_OPTION_URI_PATH);
    if (GG_FAILED(result)) {
        GG_LOG_WARNING("GG_Coap_SplitPathOrQuery failed (%d)", result);
        GG_FreeMemory(*emitter);
        return result;
    }
    (*emitter)->coap_path_options = GG_Coap_CloneOptions(coap_path_options, (*emitter)->coap_path_options_count);
    if (!(*emitter)->coap_path_options) {
        GG_FreeMemory(*emitter);
        return GG_ERROR_OUT_OF_MEMORY;
    }

    // setup the interfaces
    GG_IMPLEMENT_INTERFACE(GG_CoapEventEmitter, GG_TimerListener) {
        .OnTimerFired = GG_CoapEventEmitter_OnTimerFired
    };
    GG_SET_INTERFACE(*emitter, GG_CoapEventEmitter, GG_TimerListener);
    GG_IMPLEMENT_INTERFACE(GG_CoapEventEmitter, GG_CoapResponseListener) {
        .OnAck      = GG_CoapEventEmitter_OnAck,
        .OnError    = GG_CoapEventEmitter_OnError,
        .OnResponse = GG_CoapEventEmitter_OnResponse
    };
    GG_SET_INTERFACE(*emitter, GG_CoapEventEmitter, GG_CoapResponseListener);
    GG_IMPLEMENT_INTERFACE(GG_CoapEventEmitter, GG_BufferSource) {
        .GetDataSize = GG_CoapEventEmitter_GetRequestDataSize,
        .GetData     = GG_CoapEventEmitter_GetRequestData
    };
    GG_SET_INTERFACE(*emitter, GG_CoapEventEmitter, GG_BufferSource);

    // bind to current thread
    GG_THREAD_GUARD_BIND(*emitter);

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
void
GG_CoapEventEmitter_Destroy(GG_CoapEventEmitter* self)
{
    if (self == NULL) return;

    GG_THREAD_GUARD_CHECK_BINDING(self);

    GG_Timer_Destroy(self->timer);

    if (self->coap_request_handle) {
        GG_CoapEndpoint_CancelRequest(self->coap_endpoint, self->coap_request_handle);
    }

    if (self->coap_path_options) {
        GG_FreeMemory(self->coap_path_options);
    }

    GG_ClearAndFreeObject(self, 3);
}

//----------------------------------------------------------------------
GG_Result
GG_CoapEventEmitter_SetEvent(GG_CoapEventEmitter* self,
                             uint32_t             event_type,
                             uint32_t             max_latency)
{
    GG_ASSERT(event_type);
    GG_THREAD_GUARD_CHECK_BINDING(self);

#if defined(GG_CONFIG_ENABLE_LOGGING)
    // sanity check: we either have no events, or a request in flight, or a timer scheduled
    bool have_events = false;
    for (size_t i = 0; i < self->entry_count; i++) {
        if (self->entries[i].event_type != GG_COAP_EVENT_EMITTER_TYPE_NONE) {
            have_events = true;
            break;
        }
    }
    if (have_events && !self->coap_request_handle && !GG_Timer_IsScheduled(self->timer)) {
        GG_LOG_SEVERE("inconsistent state detected");
    };
#endif

    // compute the window bounds in terms of time relative to the scheduler's origin
    uint32_t window_end = GG_TimerScheduler_GetTime(self->timer_scheduler) + max_latency;

    // look for the entry where we're going to store this event
    GG_CoapEventEmitterEntry* insertion_point = NULL;
    for (size_t i = 0; i < self->entry_count; i++) {
        GG_CoapEventEmitterEntry* entry = &self->entries[i];

        if (entry->event_type == event_type) {
            // there is an existing entry for this event
            insertion_point = entry;
            break;
        } else if (!insertion_point && entry->event_type == GG_COAP_EVENT_EMITTER_TYPE_NONE) {
            // this spot is free
            insertion_point = entry;
        }
    }

    // check that we're able to accept this event
    if (insertion_point == NULL) {
        GG_LOG_SEVERE("no space for event");
        return GG_ERROR_OUT_OF_RESOURCES;
    }

    // now store the event info into the selected entry
    insertion_point->event_type = event_type;
    insertion_point->window_end = window_end;
    insertion_point->in_flight  = false;

    // schedule the next emission
    GG_CoapEventEmitter_Update(self);

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_Result
GG_CoapEventEmitter_UnsetEvent(GG_CoapEventEmitter* self, uint32_t event_type)
{
    GG_THREAD_GUARD_CHECK_BINDING(self);

    for (size_t i = 0; i < self->entry_count; i++) {
        if (self->entries[i].event_type == event_type) {
            self->entries[i].event_type = GG_COAP_EVENT_EMITTER_TYPE_NONE;
            self->entries[i].in_flight  = false;
            self->entries[i].window_end = 0;

            // check if this changes anything that's pending
            GG_CoapEventEmitter_Update(self);

            return GG_SUCCESS;
        }
    }

    return GG_ERROR_NO_SUCH_ITEM;
}

//----------------------------------------------------------------------
bool
GG_CoapEventEmitter_EventIsSet(GG_CoapEventEmitter* self, uint32_t event_type)
{
    GG_THREAD_GUARD_CHECK_BINDING(self);

    for (size_t i = 0; i < self->entry_count; i++) {
        if (self->entries[i].event_type == event_type) {
            return true;
        }
    }

    return false;
}

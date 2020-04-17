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
 */

#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

//! @addtogroup Utils Utilities
//! CoAP event emitter
//! @{

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdint.h>

#include "xp/common/gg_common.h"
#include "xp/coap/gg_coap.h"

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/

/**
 * A CoAP Event Emitter is a service object that is responsible for notifying a remote entity
 * of events, over CoAP.
 * Events have a type ID.
 * At any point in time, an event can be set or not set.
 * The Event Emitter keeps a current set of events that are set. This is a set, not a list
 * or queue, so the Event Emitter only keeps track, for each event type, of whether the event
 * is set or not.
 * The Event Emitter guarantees the delivery of the list of currently set events to the remote
 * CoAP endpoint. This means that it will continuously attempt to deliver a CoAP request to
 * the remote endpoint until a response is received. Retry attempts are a combination of
 * CoAP-level retransmissions (automatic retransmission with exponential back-off) and
 * service-level resends (i.e if the CoAP-level retransmissions time out, the service will
 * send a new request). The only exception to that rule is when the CoAP server returns a
 * response that indicates that there is a programming error or a misconfiguration (a 4.XX
 * response), in which case the request is not retried.
 * When a response from the remote is received, all events that where in the "in-flight"
 * set are no longer set (until a call to GG_CoapEventEmitter_SetEvent for that event type).
 */
typedef struct GG_CoapEventEmitter GG_CoapEventEmitter;

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/
// Maximum number of path segments (ex: a/b/c/d -> 4 segments)
#define GG_COAP_EVENT_EMITTER_MAX_PATH_SEGMENTS 4

// Default value for how long to wait before we retry a request (in ms)
#define GG_COAP_EVENT_EMITTER_DEFAULT_RETRY_DELAY       (30 * GG_MILLISECONDS_PER_SECOND) // 30 seconds

// Default minimum age of a request before we can cancel it (in ms)
#define GG_COAP_EVENT_EMITTER_DEFAULT_MIN_REQUEST_AGE   (5 * GG_MILLISECONDS_PER_SECOND) // 5 seconds

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/
/**
 * Create a CoAP event emitter.
 *
 * @param coap_endpoint CoAP endpoint through which to POST the events.
 * @param coap_path CoAP resource path to POST to (at most GG_COAP_EVENT_EMITTER_MAX_PATH_SEGMENTS segments).
 * @param timer_scheduler Timer scheduler used to create and schedule internal timers.
 * @param max_events Maximum number of pending events.
 * @param retry_delay How long to wait before retrying a request after a failure (in millisecond).
 * Pass 0 to use the default value (GG_COAP_EVENT_EMITTER_DEFAULT_RETRY_DELAY).
 * @param min_request_age Minimum age of a request below which the event emitter won't cancel an in-flight request
 * (in milliseconds). Pass 0 to use the default value (GG_COAP_EVENT_EMITTER_DEFAULT_MIN_REQUEST_AGE).
 * @param [out] emitter Pointer to where the created object will be returned.
 *
 * @return GG_SUCCESS if the object could be created, or a negative error code.
 */
GG_Result GG_CoapEventEmitter_Create(GG_CoapEndpoint*      coap_endpoint,
                                     const char*           coap_path,
                                     GG_TimerScheduler*    timer_scheduler,
                                     size_t                max_events,
                                     uint32_t              retry_delay,
                                     uint32_t              min_request_age,
                                     GG_CoapEventEmitter** emitter);

/**
 * Destroy the object.
 *
 * @param self The object on which this method is invoked.
 */
void GG_CoapEventEmitter_Destroy(GG_CoapEventEmitter* self);

/**
 * Set an event.
 *
 * @param self The object on which this method is invoked.
 * @param event_type The event to set.
 * @param max_latency The maximum time, in milliseconds, that the emitter may wait before
 * sending a update to the server (this allows coalescing multiple events together).
 *
 * @return GG_SUCCESS if the event could be set, or a negative error code.
 */
GG_Result GG_CoapEventEmitter_SetEvent(GG_CoapEventEmitter* self,
                                       uint32_t             event_type,
                                       uint32_t             max_latency);

/**
 * Unset an event.
 *
 * @param self The object on which this method is invoked.
 * @param event_type The event to unset.
 *
 * @return GG_SUCCESS if the event could be unset, or a negative error code.
 */
GG_Result GG_CoapEventEmitter_UnsetEvent(GG_CoapEventEmitter* self, uint32_t event_type);

/**
 * Returns whether an event is set or not.
 *
 * @param self The object on which this method is invoked.
 * @param event_type The event to query.
 *
 * @return `true` if the event is set, or `false` if it isn't.
 */
bool GG_CoapEventEmitter_EventIsSet(GG_CoapEventEmitter* self, uint32_t event_type);

//!@}

#if defined(__cplusplus)
}
#endif

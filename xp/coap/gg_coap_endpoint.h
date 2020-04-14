/**
 *
 * @file
 *
 * @copyright
 * Copyright 2017 by Fitbit, Inc., all rights reserved.
 *
 * @author Gilles Boccon-Gibod
 *
 * @date 2018-05-28
 *
 * @details
 *
 * CoAP library - endpoints (internal header)
 */

#pragma once

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
#include "xp/common/gg_lists.h"
#include "xp/common/gg_results.h"
#include "xp/common/gg_buffer.h"
#include "xp/common/gg_threads.h"
#include "xp/coap/gg_coap.h"

#if defined(GG_COAP_ENDPOINT_PRIVATE)

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/
#define GG_COAP_ACK_TIMEOUT_MS      5000 ///< should be 2000 according to RFC 7252, but set it higher for now
#define GG_COAP_ACK_RANDOM_FACTOR   1.5  ///< Ack Timeout Random Factor (RFC 7252)

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
/**
 * Implementation details of a GG_CoapEndpoint object
 * (only visible to files that define GG_COAP_ENDPOINT_PRIVATE)
 */
struct GG_CoapEndpoint {
    GG_IMPLEMENTS(GG_DataSink);
    GG_IMPLEMENTS(GG_DataSinkListener);
    GG_IMPLEMENTS(GG_DataSource);
    GG_IF_INSPECTION_ENABLED(GG_IMPLEMENTS(GG_Inspectable);)

    GG_DataSinkListener*   sink_listener;
    GG_DataSink*           connection_sink;
    GG_DataSource*         connection_source;
    GG_TimerScheduler*     timer_scheduler;
    GG_LinkedList          requests;
    size_t                 token_prefix_size; // optional token prefix size, up to 4 bytes
    uint8_t                token_prefix[4];   // optional token prefix bytes
    uint64_t               token_counter;     // counter used to generate unique tokens, sequentially
    uint16_t               message_id_counter;
    GG_LinkedList          handlers;
    GG_CoapRequestHandler* default_handler;
    GG_LinkedList          request_filters;
    bool                   locked; ///< Set to true to prevent mutating lists while iterating

    // support for keeping track of blockwise requests
    GG_LinkedList          blockwise_requests;
    uint64_t               blockwise_request_handle_base;

    GG_THREAD_GUARD_ENABLE_BINDING
};

#endif

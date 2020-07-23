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
 * @date 2017-10-10
 *
 * @details
 *
 * CoAP library implementation - endpoints
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <string.h>
#include <inttypes.h>

#define GG_COAP_ENDPOINT_PRIVATE
#define GG_COAP_MESSAGE_PRIVATE

#include "gg_coap.h"
#include "gg_coap_endpoint.h"
#include "gg_coap_message.h"
#include "gg_coap_blockwise.h"
#include "xp/common/gg_port.h"
#include "xp/common/gg_logging.h"
#include "xp/common/gg_memory.h"
#include "xp/common/gg_utils.h"
#include "xp/common/gg_threads.h"
#include "xp/common/gg_timer.h"
#include "xp/sockets/gg_sockets.h"

/*----------------------------------------------------------------------
|   logging
+---------------------------------------------------------------------*/
GG_SET_LOCAL_LOGGER("gg.xp.coap.endpoint")

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/

/**
 * State of a request.
 */
typedef enum {
    GG_COAP_REQUEST_STATE_READY_TO_SEND,   ///< The request datagram is ready to be sent to the connection sink
    GG_COAP_REQUEST_STATE_WAITING_FOR_ACK, ///< The request has been sent, no ACK or response has been received yet
    GG_COAP_REQUEST_STATE_ACKED,           ///< An ACK has been received already
    GG_COAP_REQUEST_STATE_CANCELLED        ///< The request is no longer alive and can be cleaned up
} GG_CoapRequestState;

/**
 * Object used to keep track of the context associated with a request.
 */
typedef struct {
    GG_IMPLEMENTS(GG_TimerListener);

    GG_LinkedListNode        list_node;        ///< List node to allow linking this object
    GG_CoapEndpoint*         endpoint;         ///< Endpoint to which the object belongs
    GG_CoapRequestHandle     handle;           ///< Handle used to identify the request when cancelling
    GG_CoapMessage*          message;          ///< Message representing the request
    GG_CoapRequestState      state;            ///< Current state of the request
    GG_Timer*                resend_timer;     ///< Timer used to resend the request after a certain time
    uint32_t                 resend_timeout;   ///< Timeout after which we need to resend, in ms
    uint8_t                  resend_count;     ///< Number of times we've already resent
    uint8_t                  max_resend_count; ///< Maximum number of resends
    GG_CoapResponseListener* listener;         ///< Listener for ack/error/response
} GG_CoapRequestContext;

/**
 * Object used to respond to requests asynchronously
 */
struct GG_CoapResponder {
    GG_CoapEndpoint*   endpoint;         ///< Endpoint to which the responder belongs
    GG_CoapMessage*    request;          ///< Request to which this object is responding
    GG_BufferMetadata* request_metadata; ///< Request metadata (NULL or socket address)
};

/*----------------------------------------------------------------------
|   forward declarations
+---------------------------------------------------------------------*/
static void GG_CoapEndpoint_SendPendingRequests(GG_CoapEndpoint* self);
static void GG_CoapEndpoint_SendPendingResponses(GG_CoapEndpoint* self);
static void GG_CoapEndpoint_CleanupCancelledRequests(GG_CoapEndpoint* self);

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

#if defined(GG_CONFIG_ENABLE_LOGGING)
//----------------------------------------------------------------------
static void GG_CoapEndpoint_LogMessage(const GG_CoapMessage* message, int level) {
    unsigned int code = GG_CoapMessage_GetCode(message);
    GG_LOG_LL(_GG_LocalLogger,
              level,
              "MSG code         = %d.%02d",
              GG_COAP_MESSAGE_CODE_CLASS(code),
              GG_COAP_MESSAGE_CODE_DETAIL(code));
    const char* type_str = "";
    switch (GG_CoapMessage_GetType(message)) {
        case GG_COAP_MESSAGE_TYPE_CON:
            type_str = "CON";
            break;

        case GG_COAP_MESSAGE_TYPE_NON:
            type_str = "NON";
            break;

        case GG_COAP_MESSAGE_TYPE_ACK:
            type_str = "ACK";
            break;

        case GG_COAP_MESSAGE_TYPE_RST:
            type_str = "RST";
            break;
    }
    GG_LOG_LL(_GG_LocalLogger, level, "MSG type         = %s", type_str);
    GG_LOG_LL(_GG_LocalLogger, level, "MSG id           = %d", GG_CoapMessage_GetMessageId(message));
    uint8_t token[GG_COAP_MESSGAGE_MAX_TOKEN_LENGTH];
    size_t  token_length = GG_CoapMessage_GetToken(message, token);
    char    token_hex[2 * GG_COAP_MESSGAGE_MAX_TOKEN_LENGTH + 1];
    GG_BytesToHex(token, token_length, token_hex, true);
    token_hex[2 * token_length] = 0;
    GG_LOG_LL(_GG_LocalLogger, level, "MSG token        = %s", token_hex);

    GG_CoapMessageOptionIterator option_iterator;
    GG_CoapMessage_InitOptionIterator(message, GG_COAP_MESSAGE_OPTION_ITERATOR_FILTER_ANY, &option_iterator);
    while (option_iterator.option.number) {
        switch (option_iterator.option.type) {
            case GG_COAP_MESSAGE_OPTION_TYPE_UINT:
                GG_LOG_LL(_GG_LocalLogger,
                          level,
                          "MSG option %u (uint): %u",
                          (int)option_iterator.option.number,
                          (int)option_iterator.option.value.uint);
                break;

            case GG_COAP_MESSAGE_OPTION_TYPE_STRING:
                GG_LOG_LL(_GG_LocalLogger,
                          level,
                          "MSG option %u (string): %.*s",
                          (int)option_iterator.option.number,
                          (int)option_iterator.option.value.string.length,
                          option_iterator.option.value.string.chars);
                break;

            case GG_COAP_MESSAGE_OPTION_TYPE_OPAQUE:
                GG_LOG_LL(_GG_LocalLogger,
                          level,
                          "MSG option %u (opaque): size=%u",
                          (int)option_iterator.option.number,
                          (int)option_iterator.option.value.opaque.size);
                break;

            case GG_COAP_MESSAGE_OPTION_TYPE_EMPTY:
                GG_LOG_LL(_GG_LocalLogger, level, "MSG option %u (empty)", (int)option_iterator.option.number);
                break;
        }

        GG_CoapMessage_StepOptionIterator(message, &option_iterator);
    }

    GG_LOG_LL(_GG_LocalLogger, level, "MSG payload size = %d", (int)GG_CoapMessage_GetPayloadSize(message));
}
#endif

//----------------------------------------------------------------------
static void
GG_CoapRequestContext_Destroy(GG_CoapRequestContext* self)
{
    if (self == NULL) return;

    GG_Timer_Destroy(self->resend_timer);
    GG_CoapMessage_Destroy(self->message);

    GG_ClearAndFreeObject(self, 1);
}

//----------------------------------------------------------------------
static void
GG_CoapRequestContext_Cancel(GG_CoapRequestContext* self)
{
    if (self->endpoint->locked) {
        // we're iterating, so just mark the context as cancelled, it will be destroyed later
        self->state = GG_COAP_REQUEST_STATE_CANCELLED;
    } else {
        // unlink
        GG_LINKED_LIST_NODE_REMOVE(&self->list_node);

        // destroy
        GG_CoapRequestContext_Destroy(self);
    }
}

//----------------------------------------------------------------------
//  Notify the listener of an error and terminate in a safe way
//----------------------------------------------------------------------
static void
GG_CoapRequestContext_NotifyErrorAndTerminate(GG_CoapRequestContext* self, GG_Result error, const char* message)
{
    // make a copy of the fields we'll need after the callback, in case the callback destroys us
    GG_CoapEndpoint*     endpoint = self->endpoint;
    GG_CoapRequestHandle handle   = self->handle;

    // notify the listener
    GG_CoapResponseListener_OnError(self->listener, error, message);

    // cancel using the handle, because `self` may no longer be valid
    GG_CoapEndpoint_CancelRequest(endpoint, handle);
}

//----------------------------------------------------------------------
//  Try to send out the request datagram if one is ready and pending
//----------------------------------------------------------------------
static GG_Result
GG_CoapRequestContext_TryToSend(GG_CoapRequestContext* self)
{
    // check that we're in the right state and that we have a connection sink
    if (self->state != GG_COAP_REQUEST_STATE_READY_TO_SEND || !self->endpoint->connection_sink) {
        return GG_SUCCESS;
    }

    // get the datagram so send
    GG_Buffer* datagram = NULL;
    GG_Result  result   = GG_CoapMessage_ToDatagram(self->message, &datagram);
    if (GG_FAILED(result)) {
        return result;
    }

    // try to send the datagram
    result = GG_DataSink_PutData(self->endpoint->connection_sink, datagram, NULL);
    if (result == GG_ERROR_WOULD_BLOCK) {
        // we can't send yet, we'll retry later
        GG_LOG_FINE("cannot send now, will retry later");
    } else {
        if (GG_FAILED(result)) {
            GG_LOG_WARNING("failed to send datagram (%d)", result);
            GG_CoapRequestContext_NotifyErrorAndTerminate(self, GG_ERROR_COAP_SEND_FAILURE, NULL);

            // don't return an error, since we cleaned up already
            result = GG_SUCCESS;
        } else {
            // we were able to send, now wait for an ACK
#if defined(GG_CONFIG_ENABLE_LOGGING)
            GG_CoapEndpoint_LogMessage(self->message, GG_LOG_LEVEL_FINER);
#endif
            GG_LOG_FINE("request sent, now waiting for ACK");
            self->state = GG_COAP_REQUEST_STATE_WAITING_FOR_ACK;
        }
    }
    GG_Buffer_Release(datagram);

    return result;
}

//----------------------------------------------------------------------
//  Schedule the resend timer for a request
//----------------------------------------------------------------------
static void
GG_CoapRequestContext_ScheduleTimer(GG_CoapRequestContext* self)
{
    GG_LOG_FINER("scheduling resend timer for %"PRIu64": %u ms", (uint64_t)self->handle, (int)self->resend_timeout);
    GG_Timer_Schedule(self->resend_timer, GG_CAST(self, GG_TimerListener), self->resend_timeout);
}

//----------------------------------------------------------------------
//  Called when the request resend timer has fired
//----------------------------------------------------------------------
static void
GG_CoapRequestContext_OnTimerFired(GG_TimerListener* _self, GG_Timer* timer, uint32_t elapsed)
{
    GG_CoapRequestContext* self = GG_SELF(GG_CoapRequestContext, GG_TimerListener);
    GG_COMPILER_UNUSED(timer);
    GG_COMPILER_UNUSED(elapsed);

    GG_LOG_FINE("request resend timer fired for %"PRIu64" (count = %d)",
                (uint64_t)self->handle,
                (int)self->resend_count);
    if (self->resend_count < self->max_resend_count) {
        // compute the new timeout
        self->resend_timeout *= 2;

        // mark that we're ready to send
        self->state = GG_COAP_REQUEST_STATE_READY_TO_SEND;

        // reschedule the timer
        GG_CoapRequestContext_ScheduleTimer(self);

        // update the counter
        ++self->resend_count;

        // try to send now (also give a chance to other pending/ready requests to be sent)
        GG_CoapEndpoint_SendPendingRequests(self->endpoint);
    } else {
        // we've reached the max resend count, just give up
        GG_LOG_INFO("max resend count reached, giving up");
        GG_CoapRequestContext_NotifyErrorAndTerminate(self, GG_ERROR_TIMEOUT, NULL);
    }
}

//----------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(GG_CoapRequestContext, GG_TimerListener) {
    .OnTimerFired = GG_CoapRequestContext_OnTimerFired
};

//----------------------------------------------------------------------
static GG_Result
GG_CoapRequestContext_Create(GG_CoapEndpoint*               endpoint,
                             GG_CoapResponseListener*       listener,
                             const GG_CoapClientParameters* client_parameters,
                             GG_CoapRequestContext**        request_context)
{
    // create a request context
    *request_context = (GG_CoapRequestContext*)GG_AllocateZeroMemory(sizeof(GG_CoapRequestContext));
    if (*request_context == NULL) {
        return GG_ERROR_OUT_OF_MEMORY;
    }
    GG_CoapRequestContext* self = *request_context;

    // initialize the new object
    self->endpoint = endpoint;
    self->state    = GG_COAP_REQUEST_STATE_READY_TO_SEND;
    self->listener = listener;

    // assign a handle for this request
    self->handle = endpoint->token_counter;
    if (self->handle == GG_COAP_INVALID_REQUEST_HANDLE) {
        self->handle = ++endpoint->token_counter;
    }

    // set client parameters
    if (client_parameters) {
        self->resend_timeout   = client_parameters->ack_timeout;
        self->max_resend_count = (uint8_t)client_parameters->max_resend_count;
    } else {
        // set default values
        self->max_resend_count = GG_COAP_DEFAULT_MAX_RETRANSMIT;
    }

    if (self->resend_timeout == 0) {
        // pick a random value for the resend timeout
        uint32_t random_range = (uint32_t)(GG_COAP_ACK_TIMEOUT_MS * (GG_COAP_ACK_RANDOM_FACTOR - 1.0));
        self->resend_timeout  = (uint16_t)(GG_COAP_ACK_TIMEOUT_MS + (GG_GetRandomInteger() % random_range));
    }

    // create a timer
    GG_Result result = GG_TimerScheduler_CreateTimer(endpoint->timer_scheduler, &self->resend_timer);
    if (GG_FAILED(result)) {
        GG_FreeMemory(self);
        return GG_ERROR_OUT_OF_MEMORY;
    }

    // setup the interface
    GG_SET_INTERFACE(self, GG_CoapRequestContext, GG_TimerListener);

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
//  Add a response datagram to the response queue.
//  When this function returns GG_SUCCESS, ownership of the response
//  has been transferred, and the metadata has been cloned.
//----------------------------------------------------------------------
static GG_Result
GG_CoapEndpoint_EnqueueResponse(GG_CoapEndpoint* self, GG_Buffer* response, const GG_BufferMetadata* metadata)
{
    GG_ASSERT(self->responses.cursor <  GG_ARRAY_SIZE(self->responses.datagrams));
    GG_ASSERT(self->responses.count  <= GG_ARRAY_SIZE(self->responses.datagrams));

    // check if there's space in the queue
    if (self->responses.count == GG_ARRAY_SIZE(self->responses.datagrams)) {
        return GG_ERROR_OUT_OF_RESOURCES;
    }

    // clone the metadata
    GG_BufferMetadata* metadata_clone = NULL;
    GG_Result result = GG_BufferMetadata_Clone(metadata, &metadata_clone);
    if (GG_FAILED(result)) {
        return result;
    }

    // add the response to the circular queue
    size_t tail = (self->responses.cursor + self->responses.count) %
                  GG_ARRAY_SIZE(self->responses.datagrams);
    self->responses.datagrams[tail] = response;
    self->responses.metadata[tail] = metadata_clone;
    ++self->responses.count;
    GG_LOG_FINE("enqued at %u", (int)tail);

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
// Send as many queued responses as possible
//----------------------------------------------------------------------
static void
GG_CoapEndpoint_SendPendingResponses(GG_CoapEndpoint* self)
{
    while (self->responses.count) {
        GG_Buffer*         datagram = self->responses.datagrams[self->responses.cursor];
        GG_BufferMetadata* metadata = self->responses.metadata[self->responses.cursor];

        GG_LOG_FINE("processing queued response %u (count = %u)",
                    (int)self->responses.cursor,
                    (int)self->responses.count);

        if (self->connection_sink) {
            GG_Result result = GG_DataSink_PutData(self->connection_sink, datagram, metadata);
            if (GG_SUCCEEDED(result)) {
                GG_LOG_FINE("sent");
            } else {
                if (result == GG_ERROR_WOULD_BLOCK) {
                    // stop trying
                    GG_LOG_FINE("would block, stopping");
                    return;
                } else {
                    // move on
                    GG_LOG_FINE("sink error, dropping (%d)", result);
                }
            }
        }

        // remove from the queue
        --self->responses.count;
        self->responses.cursor = (self->responses.cursor + 1) % GG_ARRAY_SIZE(self->responses.datagrams);
        GG_Buffer_Release(datagram);
        if (metadata) {
            GG_FreeMemory(metadata);
        }
    }
}

//----------------------------------------------------------------------
// Try to send a response
//----------------------------------------------------------------------
static GG_Result
GG_CoapEndpoint_SendResponse(GG_CoapEndpoint* self, const GG_CoapMessage* response, const GG_BufferMetadata* metadata)
{
#if defined(GG_CONFIG_ENABLE_LOGGING)
    GG_LOG_FINER("trying to send response (%u in queue)", (int)self->responses.count);
    GG_CoapEndpoint_LogMessage(response, GG_LOG_LEVEL_FINER);
#endif

    // first try to send any pending responses
    GG_CoapEndpoint_SendPendingResponses(self);

    // drop the response if there's no sink
    if (!self->connection_sink) {
        GG_LOG_FINE("no sink, dropping");
        return GG_SUCCESS;
    }

    // convert the message to a datagram
    GG_Buffer* datagram = NULL;
    GG_Result  result   = GG_CoapMessage_ToDatagram(response, &datagram);
    if (GG_FAILED(result)) {
        return result;
    }

    // if the queue is empty, try to send right away
    if (self->responses.count == 0) {
        result = GG_DataSink_PutData(self->connection_sink, datagram, metadata);
        if (GG_SUCCEEDED(result)) {
            // yay! the response was sent
            GG_LOG_FINE("response sent");
            GG_Buffer_Release(datagram);
            return GG_SUCCESS;
        }
        if (result != GG_ERROR_WOULD_BLOCK) {
            // something went wrong, drop the response
            GG_LOG_WARNING("failed to send to the sink, dropping (%d)", result);
            GG_Buffer_Release(datagram);
            return GG_SUCCESS;
        }

        GG_LOG_FINE("sink would block");
    }

    // we can't send the response at this time, enqueue it, we'll try again later
    result = GG_CoapEndpoint_EnqueueResponse(self, datagram, metadata);
    if (GG_FAILED(result)) {
        GG_LOG_WARNING("failed to enqueue response (%d)", result);
    GG_Buffer_Release(datagram);
        return result;
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_Result
GG_CoapResponder_CreateResponse(GG_CoapResponder*          self,
                                uint8_t                    code,
                                GG_CoapMessageOptionParam* options,
                                size_t                     options_count,
                                const uint8_t*             payload,
                                size_t                     payload_size,
                                GG_CoapMessage**           response)
{
    return GG_CoapEndpoint_CreateResponse(self->endpoint,
                                          self->request,
                                          code,
                                          options,
                                          options_count,
                                          payload,
                                          payload_size,
                                          response);
}

//----------------------------------------------------------------------
GG_Result
GG_CoapResponder_SendResponse(GG_CoapResponder* self, GG_CoapMessage* response)
{
    // send the response
    return GG_CoapEndpoint_SendResponse(self->endpoint, response, self->request_metadata);
}

//----------------------------------------------------------------------
GG_Result
GG_CoapResponder_Respond(GG_CoapResponder*          self,
                         uint8_t                    code,
                         GG_CoapMessageOptionParam* options,
                         size_t                     options_count,
                         const uint8_t*             payload,
                         size_t                     payload_size)
{
    GG_CoapMessage* response = NULL;
    GG_Result result = GG_CoapResponder_CreateResponse(self,
                                                       code,
                                                       options,
                                                       options_count,
                                                       payload,
                                                       payload_size,
                                                       &response);
    if (GG_SUCCEEDED(result)) {
        result = GG_CoapResponder_SendResponse(self, response);
    }

    GG_CoapMessage_Destroy(response);
    return result;
}

//----------------------------------------------------------------------
void
GG_CoapResponder_Release(GG_CoapResponder* self)
{
    // destroy the request
    GG_CoapMessage_Destroy(self->request);

    // destroy ourself
    if (self->request_metadata) {
        GG_FreeMemory(self->request_metadata);
    }
    GG_FreeMemory(self);
}

//----------------------------------------------------------------------
// Create a responder.
// The ownership of the request object is transferred to the newly created
// object. The request field can be set to NULL subsequently if the ownership
// of the request should no longer be with this object.
//----------------------------------------------------------------------
static GG_Result
GG_CoapEndpoint_CreateResponder(GG_CoapEndpoint*         self,
                                GG_CoapMessage*          request,
                                const GG_BufferMetadata* metadata,
                                GG_CoapResponder**       responder)
{
    // allocate a new object
    *responder = GG_AllocateZeroMemory(sizeof(GG_CoapResponder));
    if (*responder == NULL) {
        return GG_ERROR_OUT_OF_MEMORY;
    }

    // initialize the object
    (*responder)->endpoint = self;
    (*responder)->request  = request;
    GG_BufferMetadata_Clone(metadata, &(*responder)->request_metadata);

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
// Handle a request.
//
// Returns `true` if the request was fully handled (and thus the request object can be released)
// or `false` if the request was partially handled (an async response will come later, and thus
// the request must not be released yet, as its ownership was transferred to the responder object)
//----------------------------------------------------------------------
static bool
GG_CoapEndpoint_OnRequest(GG_CoapEndpoint* self, GG_CoapMessage* request, const GG_BufferMetadata* metadata)
{
    GG_CoapMessage* response      = NULL;
    bool            fully_handled = true;
    GG_Result       result        = GG_FAILURE;

    // setup the response metadata based on the request metadata
    GG_SocketAddressMetadata response_metadata;
    if (metadata) {
        if (metadata->type == GG_BUFFER_METADATA_TYPE_SOURCE_SOCKET_ADDRESS) {
            // send the response back to where the request came from
            const GG_SocketAddressMetadata* socket_metadata = (const GG_SocketAddressMetadata*)metadata;
            char                            socket_address_str[20];
            GG_SocketAddress_AsString(&socket_metadata->socket_address, socket_address_str, sizeof(socket_address_str));
            GG_LOG_FINER("handling request from %s", socket_address_str);
            response_metadata           = *socket_metadata;
            response_metadata.base.type = GG_BUFFER_METADATA_TYPE_DESTINATION_SOCKET_ADDRESS;
            metadata                    = &response_metadata.base;
        } else {
            // we don't need this metadata for the response
            metadata = NULL;
        }
    }

    // prepare a default handler if needed
    GG_CoapRequestHandlerNode* handler;
    GG_CoapRequestHandlerNode  default_handler_node;
    if (self->default_handler) {
        default_handler_node = (GG_CoapRequestHandlerNode) {
            .path    = "/",
            .flags   = GG_COAP_REQUEST_HANDLER_FLAGS_ALLOW_ALL,
            .handler = self->default_handler
        };
        handler = &default_handler_node;
    } else {
        handler = NULL;
    }

    // look for a handler
    GG_LINKED_LIST_FOREACH(node, &self->handlers) {
        GG_CoapRequestHandlerNode* handler_node = GG_LINKED_LIST_ITEM(node, GG_CoapRequestHandlerNode, list_node);

        // try to match the path segment by segment
        const char*                  path = handler_node->path;
        GG_CoapMessageOptionIterator iterator;
        GG_CoapMessage_InitOptionIterator(request, GG_COAP_MESSAGE_OPTION_URI_PATH, &iterator);
        bool match = false;
        GG_LOG_FINEST("looking for a match for handler at path %s", path);
        while (iterator.option.type == GG_COAP_MESSAGE_OPTION_TYPE_STRING) {
            // check that all the chars of the uri path segment match
            match = true;
            for (size_t i = 0; i < iterator.option.value.string.length; i++) {
                if (path[i] == '\0' || path[i] != iterator.option.value.string.chars[i]) {
                    // mismatch
                    match = false;
                    break;
                }
            }
            if (!match) {
                break;
            }

            // check that the uri path segment is completely matched
            path += iterator.option.value.string.length;
            if (*path == '\0') {
                // end of the path, consider this a match (prefix)
                break;
            } else if (*path == '/') {
                // we matched a segment, but there are more
                ++path;
                match = false;
            } else {
                // the path segment isn't
                match = false;
                break;
            }

            // move on to the next uri path segment, if any
            GG_CoapMessage_StepOptionIterator(request, &iterator);
        }

        if (match) {
            // handler found
            handler = handler_node;

            // done looking for a handler
            break;
        }
    }

    if (handler) {
        // check that the method matches the filter
        uint32_t method_flag = 0;
        uint8_t  method      = GG_CoapMessage_GetCode(request);
        switch (method) {
            case GG_COAP_METHOD_GET:
                method_flag = GG_COAP_REQUEST_HANDLER_FLAG_ALLOW_GET;
                break;

            case GG_COAP_METHOD_POST:
                method_flag = GG_COAP_REQUEST_HANDLER_FLAG_ALLOW_POST;
                break;

            case GG_COAP_METHOD_PUT:
                method_flag = GG_COAP_REQUEST_HANDLER_FLAG_ALLOW_PUT;
                break;

            case GG_COAP_METHOD_DELETE:
                method_flag = GG_COAP_REQUEST_HANDLER_FLAG_ALLOW_DELETE;
                break;

            default:
                break;
        }
        if (handler->flags & method_flag) {
            // invoke the filters first
            result = GG_SUCCESS;
            GG_LINKED_LIST_FOREACH(node, &self->request_filters) {
                GG_CoapRequestFilterNode* filter_node = GG_LINKED_LIST_ITEM(node, GG_CoapRequestFilterNode, list_node);
                result = GG_CoapRequestFilter_FilterRequest(filter_node->filter,
                                                            self,
                                                            handler->flags,
                                                            request,
                                                            &response);
                if (result || response) {
                    GG_LOG_FINER("a filter terminated the chain with result %d", result);
                    break;
                }
            }
            GG_ASSERT(GG_SUCCEEDED(result) || response == NULL);

            // invoke the handler if the filters didn't terminate the chain
            if (GG_SUCCEEDED(result) && response == NULL) {
                // create a responder object for async-enabled handers
                GG_CoapResponder* responder = NULL;
                if (handler->flags & GG_COAP_REQUEST_HANDLER_FLAG_ENABLE_ASYNC) {
                    result = GG_CoapEndpoint_CreateResponder(self, request, metadata, &responder);
                    if (GG_FAILED(result)) {
                        // bad condition, don't even try to respond
                        GG_LOG_SEVERE("GG_CoapResponder_Create failed (%d)", result);
                        return true;
                    }
                }

                GG_LOG_FINE("invoking handler");
                result = GG_CoapRequestHandler_OnRequest(handler->handler,
                                                         self,
                                                         request,
                                                         responder,
                                                         metadata,
                                                         &response);
                GG_ASSERT(result || response);
                if (result != GG_SUCCESS) {
                    GG_ASSERT(response == NULL);
                    if (result > 0) {
                        GG_LOG_FINE("request handler returned %d", result);
                    } else if (result == GG_ERROR_WOULD_BLOCK) {
                        if (handler->flags & GG_COAP_REQUEST_HANDLER_FLAG_ENABLE_ASYNC) {
                            GG_LOG_FINE("async response");
                            fully_handled = false; // the response will be sent later using the responder
                        } else {
                            GG_LOG_FINE("response will be sent out of band");
                        }
                    } else {
                        GG_LOG_WARNING("request handler failed (%d)", result);
                    }
                }

                // release the responder if the handler isn't holding on to it
                if (responder && fully_handled) {
                    // don't leave the ownership of the request object with the responder
                    responder->request = NULL;

                    // release the responder
                    GG_CoapResponder_Release(responder);
                }
            }
        } else {
            // the method is not allowed
            GG_LOG_INFO("method %d not allowed by handler", (int)method);
            result = GG_COAP_MESSAGE_CODE_METHOD_NOT_ALLOWED;
        }
    }

    if (response == NULL && result != GG_ERROR_WOULD_BLOCK) {
        uint8_t response_code;
        if (handler == NULL) {
            // no handler
            response_code = GG_COAP_MESSAGE_CODE_NOT_FOUND;
        } else if (result > 0 && result <= 0xFF) {
            // the handler returned a positive result between 1 and 255, use that as the response code
            response_code = (uint8_t)result;
        } else {
            // all other errors are signaled as an internal server error
            response_code = GG_COAP_MESSAGE_CODE_INTERNAL_SERVER_ERROR;
        }

        result = GG_CoapEndpoint_CreateResponse(self, request, response_code, NULL, 0, NULL, 0, &response);
        if (GG_FAILED(result)) {
            // not good... if we can't create a response, we can't even send back an error
            GG_LOG_WARNING("failed to create a response (%d)", result);
        }
    }

    if (response) {
        // send the response now
        GG_CoapEndpoint_SendResponse(self, response, metadata);

        // done with the response
        GG_CoapMessage_Destroy(response);
    }

    return fully_handled;
}

//----------------------------------------------------------------------
static void
GG_CoapEndpoint_OnResponse(GG_CoapEndpoint* self, GG_CoapMessage* response)
{
    GG_CoapMessageType message_type = GG_CoapMessage_GetType(response);

    // prepare to iterate
    bool was_locked = self->locked;
    self->locked    = true;

    // find a request context with a matching token
    bool    matched = false;
    uint8_t message_token[GG_COAP_MESSGAGE_MAX_TOKEN_LENGTH];
    size_t  message_token_length = GG_CoapMessage_GetToken(response, message_token);
    GG_LINKED_LIST_FOREACH_SAFE(node, &self->requests) {
        GG_CoapRequestContext* context = GG_LINKED_LIST_ITEM(node, GG_CoapRequestContext, list_node);
        uint8_t                context_token[GG_COAP_MESSGAGE_MAX_TOKEN_LENGTH];
        size_t                 context_token_length = GG_CoapMessage_GetToken(context->message, context_token);

        // check if the token matches
        if (context_token_length == message_token_length &&
            !memcmp(context_token, message_token, context_token_length)) {
            // match!
            GG_LOG_FINE("found matching context");
            matched = true;

            // check/update the request state
            GG_LOG_FINE("request state = %d", (int)context->state);
            bool notify_ack = false;
            switch (context->state) {
                case GG_COAP_REQUEST_STATE_READY_TO_SEND:
                    // that's odd, we're receiveing a response while still waiting to send...
                    // this could happen in edge cases where we sent a request, got a timeout
                    // and tried to re-send, but couldn't yet deliver the resent datagram to
                    // the transport, and then finally got a respond for the first request
                    // that timed out.

                    // FALLTHROUGH

                case GG_COAP_REQUEST_STATE_WAITING_FOR_ACK:
                    if (message_type != GG_COAP_MESSAGE_TYPE_RST) {
                        context->state = GG_COAP_REQUEST_STATE_ACKED;
                        notify_ack     = true;
                    }
                    break;

                case GG_COAP_REQUEST_STATE_ACKED:
                case GG_COAP_REQUEST_STATE_CANCELLED:
                    break;
            }

            // stop and destroy the resend timer if we have one scheduled
            GG_Timer_Destroy(context->resend_timer);
            context->resend_timer = NULL;

            // check if this is a response or an ACK-only empty message
            bool empty_ack = (message_type == GG_COAP_MESSAGE_TYPE_ACK && GG_CoapMessage_GetCode(response) == 0);

            // notify the listener of an ACK if needed (explicit or implicit)
            if (notify_ack) {
                GG_CoapResponseListener_OnAck(context->listener);
            }

            // if this isn't just an empty ACK, notify the listener of a response or error
            if (!empty_ack) {
                if (message_type == GG_COAP_MESSAGE_TYPE_RST) {
                    GG_LOG_FINE("handling RST response");
                    GG_CoapResponseListener_OnError(context->listener, GG_ERROR_COAP_RESET, NULL);
                } else if (GG_CoapMessage_GetCode(response) == 0) {
                    GG_LOG_WARNING("invalid response (code == 0)");
                    GG_CoapResponseListener_OnError(context->listener, GG_ERROR_COAP_UNEXPECTED_MESSAGE, NULL);
                } else {
                    GG_LOG_FINE("handling response");
                    GG_CoapResponseListener_OnResponse(context->listener, response);
                }

                // this is the last step in the exchange, we're done with this context
                GG_CoapRequestContext_Cancel(context);
                context = NULL;
            }

            // TODO: deal with CON responses (non-piggybacked)

            // don't look for any more matches
            break;
        }
    }

    // dispose of the context if needed
    if (!matched) {
        GG_LOG_INFO("received unmatched message");
    }

    // cleanup if needed
    if (!was_locked) {
        self->locked = false;
        GG_CoapEndpoint_CleanupCancelledRequests(self);
    }
}

//----------------------------------------------------------------------
static GG_Result
GG_CoapEndpoint_PutData(GG_DataSink* _self, GG_Buffer* buffer, const GG_BufferMetadata* metadata)
{
    GG_CoapEndpoint* self = GG_SELF(GG_CoapEndpoint, GG_DataSink);

    // parse the datagram
    GG_CoapMessage* message = NULL;
    GG_Result result = GG_CoapMessage_CreateFromDatagram(buffer, &message);
    if (GG_FAILED(result)) {
        // TODO: maybe send back an RST. Drop for now
        GG_LOG_WARNING("invalid datagram received (%d)", result);
        return GG_SUCCESS;
    }

#if defined(GG_CONFIG_ENABLE_LOGGING)
    GG_CoapEndpoint_LogMessage(message, GG_LOG_LEVEL_FINER);
#endif

    // check if this is a request or a response
    uint8_t message_code = GG_CoapMessage_GetCode(message);
    if (GG_COAP_MESSAGE_CODE_CLASS(message_code) == GG_COAP_MESSAGE_CODE_CLASS_REQUEST) {
        // this is a request
        const GG_BufferMetadata* socket_metadata = NULL;
        if (metadata && metadata->type == GG_BUFFER_METADATA_TYPE_SOURCE_SOCKET_ADDRESS) {
            socket_metadata = metadata;
        }
        if (!GG_CoapEndpoint_OnRequest(self, message, socket_metadata)) {
            // not fully handled, prevent this message from being reclaimed now
            message = NULL;
        }
    } else {
        // this is a response
        GG_CoapEndpoint_OnResponse(self, message);
    }

    // destroy the message
    GG_CoapMessage_Destroy(message);

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
static GG_Result
GG_CoapEndpoint_SetListener(GG_DataSink* _self, GG_DataSinkListener* listener)
{
    GG_CoapEndpoint* self = GG_SELF(GG_CoapEndpoint, GG_DataSink);
    self->sink_listener = listener;

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(GG_CoapEndpoint, GG_DataSink) {
    .PutData     = GG_CoapEndpoint_PutData,
    .SetListener = GG_CoapEndpoint_SetListener
};

//----------------------------------------------------------------------
static void
GG_CoapEndpoint_OnCanPut(GG_DataSinkListener* _self)
{
    GG_CoapEndpoint* self = GG_SELF(GG_CoapEndpoint, GG_DataSinkListener);

    // try sending what's pending, starting with requests or responses
    // depending on a simple toggle for round-robin
    if (self->try_responses_first) {
        GG_CoapEndpoint_SendPendingResponses(self);
        GG_CoapEndpoint_SendPendingRequests(self);
    } else {
        GG_CoapEndpoint_SendPendingRequests(self);
        GG_CoapEndpoint_SendPendingResponses(self);
    }
    self->try_responses_first = !self->try_responses_first;
}

//----------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(GG_CoapEndpoint, GG_DataSinkListener) {
    .OnCanPut = GG_CoapEndpoint_OnCanPut
};

//----------------------------------------------------------------------
static GG_Result
GG_CoapEndpoint_SetDataSink(GG_DataSource* _self, GG_DataSink* sink)
{
    GG_CoapEndpoint* self = GG_SELF(GG_CoapEndpoint, GG_DataSource);

    // de-register as a listener from the current sink
    if (self->connection_sink) {
        GG_DataSink_SetListener(self->connection_sink, NULL);
    }

    // keep a reference to the new sink
    self->connection_sink = sink;

    if (sink) {
        // register with the sink as a listener to know when we can try to send
        GG_DataSink_SetListener(sink, GG_CAST(self, GG_DataSinkListener));

        // try to send anything that's pending
        GG_CoapEndpoint_SendPendingRequests(self);
        GG_CoapEndpoint_SendPendingResponses(self);
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(GG_CoapEndpoint, GG_DataSource) {
    .SetDataSink = GG_CoapEndpoint_SetDataSink
};

#if defined(GG_CONFIG_ENABLE_INSPECTION)
//----------------------------------------------------------------------
GG_Inspectable*
GG_CoapEndpoint_AsInspectable(GG_CoapEndpoint* self)
{
    return GG_CAST(self, GG_Inspectable);
}

//----------------------------------------------------------------------
static GG_Result
GG_CoapEndpoint_Inspect(GG_Inspectable* _self, GG_Inspector* inspector, const GG_InspectionOptions* options)
{
    GG_CoapEndpoint* self = GG_SELF(GG_CoapEndpoint, GG_Inspectable);

    // inspect handlers
    GG_Inspector_OnArrayStart(inspector, "handlers");
    GG_LINKED_LIST_FOREACH_SAFE(node, &self->handlers) {
        GG_CoapRequestHandlerNode* handler_node = GG_LINKED_LIST_ITEM(node, GG_CoapRequestHandlerNode, list_node);

        GG_Inspector_OnObjectStart(inspector, NULL);
        GG_Inspector_OnString(inspector, "path", handler_node->path);
        GG_Inspector_OnInteger(inspector, "flags", handler_node->flags, GG_INSPECTOR_FORMAT_HINT_HEX);
        GG_Inspector_OnObjectEnd(inspector);
    }
    GG_Inspector_OnArrayEnd(inspector);

    // inspect pending requests
    GG_Inspector_OnArrayStart(inspector, "requests");
    GG_LINKED_LIST_FOREACH_SAFE(node, &self->requests) {
        GG_CoapRequestContext* context = GG_LINKED_LIST_ITEM(node, GG_CoapRequestContext, list_node);

        GG_Inspector_OnObjectStart(inspector, NULL);
        GG_Inspector_OnInteger(inspector, "handle", context->handle, GG_INSPECTOR_FORMAT_HINT_NONE);
        GG_Inspector_OnInspectable(inspector, "message", GG_CoapMessage_AsInspectable(context->message));
        const char* state_string = "";
        switch (context->state) {
            case GG_COAP_REQUEST_STATE_READY_TO_SEND:
                state_string = "READY_TO_SEND";
                break;

            case GG_COAP_REQUEST_STATE_WAITING_FOR_ACK:
                state_string = "WAITING_FOR_ACK";
                break;

            case GG_COAP_REQUEST_STATE_ACKED:
                state_string = "ACKED";
                break;

            case GG_COAP_REQUEST_STATE_CANCELLED:
                state_string = "CANCELLED";
                break;
        }
        GG_Inspector_OnString(inspector, "state", state_string);
        if (context->resend_timer) {
            GG_Inspector_OnInteger(inspector,
                                   "resend_timer_remaining_time",
                                   GG_Timer_GetRemainingTime(context->resend_timer),
                                   GG_INSPECTOR_FORMAT_HINT_UNSIGNED);
        }
        GG_Inspector_OnInteger(inspector, "resend_timeout", context->resend_timeout, GG_INSPECTOR_FORMAT_HINT_UNSIGNED);
        GG_Inspector_OnObjectEnd(inspector);
    }
    GG_Inspector_OnArrayEnd(inspector);

    // inspect request filters
    GG_Inspector_OnArrayStart(inspector, "filters");
    GG_LINKED_LIST_FOREACH_SAFE(node, &self->request_filters) {
        GG_CoapRequestFilterNode* filter_node = GG_LINKED_LIST_ITEM(node, GG_CoapRequestFilterNode, list_node);
        GG_Inspector_OnInteger(inspector, NULL, (int64_t)(uintptr_t)filter_node->filter, GG_INSPECTOR_FORMAT_HINT_HEX);
    }
    GG_Inspector_OnArrayEnd(inspector);

    // inspect blockwise request contexts
    GG_CoapEndpoint_InspectBlockwiseRequestContexts(self, inspector, options);

    // inspect fields
    GG_Inspector_OnInteger(inspector,
                           "token_counter",
                           self->token_counter,
                           GG_INSPECTOR_FORMAT_HINT_UNSIGNED);
    GG_Inspector_OnInteger(inspector,
                           "message_id_counter",
                           self->message_id_counter,
                           GG_INSPECTOR_FORMAT_HINT_UNSIGNED);
    GG_Inspector_OnInteger(inspector,
                           "blockwise_request_handle_base",
                           self->blockwise_request_handle_base,
                           GG_INSPECTOR_FORMAT_HINT_UNSIGNED);

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(GG_CoapEndpoint, GG_Inspectable) {
    .Inspect = GG_CoapEndpoint_Inspect
};
#endif

//----------------------------------------------------------------------
GG_Result
GG_CoapEndpoint_Create(GG_TimerScheduler* timer_scheduler,
                       GG_DataSink*       connection_sink,
                       GG_DataSource*     connection_source,
                       GG_CoapEndpoint**  endpoint)
{
    GG_ASSERT(timer_scheduler);
    GG_ASSERT(endpoint);

    // allocate an object
    GG_CoapEndpoint* self = (GG_CoapEndpoint*)GG_AllocateZeroMemory(sizeof(GG_CoapEndpoint));
    if (self == NULL) {
        *endpoint = NULL;
        return GG_ERROR_OUT_OF_MEMORY;
    }

    // init the object
    self->connection_sink               = connection_sink;
    self->connection_source             = connection_source;
    self->timer_scheduler               = timer_scheduler;
    self->blockwise_request_handle_base = GG_COAP_INVALID_REQUEST_HANDLE + 1;
    GG_LINKED_LIST_INIT(&self->requests);
    GG_LINKED_LIST_INIT(&self->blockwise_requests);
    GG_LINKED_LIST_INIT(&self->handlers);
    GG_LINKED_LIST_INIT(&self->request_filters);

    // generate a random token base
    self->token_counter = GG_GetRandomInteger();

    // setup our interfaces
    GG_SET_INTERFACE(self, GG_CoapEndpoint, GG_DataSink);
    GG_SET_INTERFACE(self, GG_CoapEndpoint, GG_DataSinkListener);
    GG_SET_INTERFACE(self, GG_CoapEndpoint, GG_DataSource);
    GG_IF_INSPECTION_ENABLED(GG_SET_INTERFACE(self, GG_CoapEndpoint, GG_Inspectable));

    // register with the source as a sink to get incoming datagrams
    if (connection_source) {
        GG_DataSource_SetDataSink(connection_source, GG_CAST(self, GG_DataSink));
    }

    // register with the sink as a listener to know when we can try to send
    if (connection_sink) {
        GG_DataSink_SetListener(connection_sink, GG_CAST(self, GG_DataSinkListener));
    }

    // bind to the current thread
    GG_THREAD_GUARD_BIND(self);

    // return the object
    *endpoint = self;

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
void
GG_CoapEndpoint_Destroy(GG_CoapEndpoint* self)
{
    if (self == NULL) return;

    GG_THREAD_GUARD_CHECK_BINDING(self);

    // de-register as a sink from the current source
    if (self->connection_source) {
        GG_DataSource_SetDataSink(self->connection_source, NULL);
    }

    // de-register as a listener from the current sink
    if (self->connection_sink) {
        GG_DataSink_SetListener(self->connection_sink, NULL);
    }

    // cleanup blockwise request contexts
    GG_CoapEndpoint_DestroyBlockwiseRequestContexts(self);

    // cleanup pending requests
    GG_LINKED_LIST_FOREACH_SAFE(node, &self->requests) {
        GG_CoapRequestContext* context = GG_LINKED_LIST_ITEM(node, GG_CoapRequestContext, list_node);
        GG_CoapRequestContext_Destroy(context);
    }

    // cleanup handlers
    GG_LINKED_LIST_FOREACH_SAFE(node, &self->handlers) {
        GG_CoapRequestHandlerNode* handler_node = GG_LINKED_LIST_ITEM(node, GG_CoapRequestHandlerNode, list_node);
        if (handler_node->auto_release) {
            GG_FreeMemory(handler_node);
        }
    }

    // cleanup request filters
    GG_LINKED_LIST_FOREACH_SAFE(node, &self->request_filters) {
        GG_CoapRequestFilterNode* filter_node = GG_LINKED_LIST_ITEM(node, GG_CoapRequestFilterNode, list_node);
        if (filter_node->auto_release) {
            GG_FreeMemory(filter_node);
        }
    }

    GG_ClearAndFreeObject(self, 3);
}

//----------------------------------------------------------------------
GG_DataSink*
GG_CoapEndpoint_AsDataSink(GG_CoapEndpoint* self)
{
    return GG_CAST(self, GG_DataSink);
}

//----------------------------------------------------------------------
GG_DataSource*
GG_CoapEndpoint_AsDataSource(GG_CoapEndpoint* self)
{
    return GG_CAST(self, GG_DataSource);
}

//----------------------------------------------------------------------
static void
GG_CoapEndpoint_CleanupCancelledRequests(GG_CoapEndpoint* self)
{
    GG_ASSERT(!self->locked);

    // destroy all cancelled request contexts
    GG_LINKED_LIST_FOREACH_SAFE(node, &self->requests) {
        GG_CoapRequestContext* context = GG_LINKED_LIST_ITEM(node, GG_CoapRequestContext, list_node);
        if (context->state == GG_COAP_REQUEST_STATE_CANCELLED) {
            GG_LINKED_LIST_NODE_REMOVE(node);
            GG_CoapRequestContext_Destroy(context);
        }
    }
}

//----------------------------------------------------------------------
GG_Result
GG_CoapEndpoint_CancelRequest(GG_CoapEndpoint* self, GG_CoapRequestHandle request_handle)
{
    GG_THREAD_GUARD_CHECK_BINDING(self);

    GG_Result result = GG_ERROR_NO_SUCH_ITEM;
    GG_LINKED_LIST_FOREACH_SAFE(node, &self->requests) {
        GG_CoapRequestContext* context = GG_LINKED_LIST_ITEM(node, GG_CoapRequestContext, list_node);
        if (context->handle == request_handle) {
            GG_CoapRequestContext_Cancel(context);
            result = GG_SUCCESS;
            break;
        }
    }

    return result;
}

//----------------------------------------------------------------------
static void
GG_CoapEndpoint_SendPendingRequests(GG_CoapEndpoint* self)
{
    // prepare to iterate
    bool was_locked = self->locked;
    self->locked = true;

    GG_LINKED_LIST_FOREACH_SAFE(node, &self->requests) {
        GG_CoapRequestContext* context = GG_LINKED_LIST_ITEM(node, GG_CoapRequestContext, list_node);
        if (context->state == GG_COAP_REQUEST_STATE_READY_TO_SEND) {
            GG_Result result = GG_CoapRequestContext_TryToSend(context);
            if (result == GG_ERROR_WOULD_BLOCK) {
                // no point continuing in that case, we'll retry later
                GG_LOG_FINER("would block while walking pending requests, stopping now");
                break;
            }
        }
    }

    // cleanup if needed
    if (!was_locked) {
        self->locked = false;
        GG_CoapEndpoint_CleanupCancelledRequests(self);
    }
}

//----------------------------------------------------------------------
GG_Result
GG_CoapEndpoint_SendRequestFromBufferSource(GG_CoapEndpoint*               self,
                                            GG_CoapMethod                  method,
                                            GG_CoapMessageOptionParam*     options,
                                            size_t                         options_count,
                                            GG_BufferSource*               payload_source,
                                            const GG_CoapClientParameters* client_parameters,
                                            GG_CoapResponseListener*       listener,
                                            GG_CoapRequestHandle*          request_handle)
{
    GG_ASSERT(self);
    GG_THREAD_GUARD_CHECK_BINDING(self);

    // convert the token counter into a token
    uint8_t token[8];
    size_t  token_length = 0;
    if (self->token_prefix_size) {
        GG_ASSERT(self->token_prefix_size <= 4);
        memcpy(token, self->token_prefix, self->token_prefix_size);
        token_length += self->token_prefix_size;
    }
    GG_BytesFromInt32Be(&token[token_length], (uint32_t)self->token_counter++);
    token_length += 4;

    // create a request context
    GG_CoapRequestContext* request_context = NULL;
    GG_Result result = GG_CoapRequestContext_Create(self, listener, client_parameters, &request_context);
    if (GG_FAILED(result)) {
        return result;
    }
    if (request_handle) {
        *request_handle = request_context->handle;
    }

    // get the payload size
    size_t payload_size = payload_source ? GG_BufferSource_GetDataSize(payload_source) : 0;

    // create a request message
    result = GG_CoapMessage_Create((uint8_t)method,
                                   GG_COAP_MESSAGE_TYPE_CON,
                                   options,
                                   options_count,
                                   self->message_id_counter++,
                                   token,
                                   token_length,
                                   NULL,
                                   payload_size,
                                   &request_context->message);
    if (GG_FAILED(result)) {
        GG_CoapRequestContext_Destroy(request_context);
        return result;
    }

    // copy the payload
    if (payload_size) {
        GG_BufferSource_GetData(payload_source, GG_CoapMessage_UsePayload(request_context->message));
    }

    // add the request to the list of pending requests
    GG_LINKED_LIST_APPEND(&self->requests, &request_context->list_node);

    // schedule the first resend timer
    GG_CoapRequestContext_ScheduleTimer(request_context);

    // try to send any request that may be pending
    GG_CoapEndpoint_SendPendingRequests(self);

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_Result
GG_CoapEndpoint_SendRequest(GG_CoapEndpoint*               self,
                            GG_CoapMethod                  method,
                            GG_CoapMessageOptionParam*     options,
                            size_t                         options_count,
                            const uint8_t*                 payload,
                            size_t                         payload_size,
                            const GG_CoapClientParameters* client_parameters,
                            GG_CoapResponseListener*       listener,
                            GG_CoapRequestHandle*          request_handle)
{
    // init a static buffer source to represent the payload
    GG_StaticBufferSource payload_buffer;
    GG_StaticBufferSource_Init(&payload_buffer, payload, payload_size);

    // send the request
    return GG_CoapEndpoint_SendRequestFromBufferSource(self,
                                                       method,
                                                       options,
                                                       options_count,
                                                       GG_StaticBufferSource_AsBufferSource(&payload_buffer),
                                                       client_parameters,
                                                       listener,
                                                       request_handle);
}

//----------------------------------------------------------------------
GG_Result
GG_CoapEndpoint_CreateResponse(GG_CoapEndpoint*           self,
                               const GG_CoapMessage*      request,
                               uint8_t                    code,
                               GG_CoapMessageOptionParam* options,
                               size_t                     options_count,
                               const uint8_t*             payload,
                               size_t                     payload_size,
                               GG_CoapMessage**           response)
{
    GG_ASSERT(self);
    GG_ASSERT(request);
    GG_ASSERT(response);
    GG_THREAD_GUARD_CHECK_BINDING(self);

    // get the request token
    uint8_t token[GG_COAP_MESSGAGE_MAX_TOKEN_LENGTH];
    size_t  token_length = GG_CoapMessage_GetToken(request, token);

    // create the response message
    return GG_CoapMessage_Create(code,
                                 GG_COAP_MESSAGE_TYPE_ACK,
                                 options,
                                 options_count,
                                 GG_CoapMessage_GetMessageId(request),
                                 token,
                                 token_length,
                                 payload,
                                 payload_size,
                                 response);
}

//----------------------------------------------------------------------
GG_Result
GG_CoapEndpoint_RegisterRequestHandlerNode(GG_CoapEndpoint*           self,
                                           const char*                path,
                                           uint32_t                   flags,
                                           GG_CoapRequestHandlerNode* handler_node)
{
    GG_ASSERT(self);
    GG_ASSERT(path);
    GG_ASSERT(handler_node);
    GG_THREAD_GUARD_CHECK_BINDING(self);

    // skip any leading / character
    while (*path == '/') {
        ++path;
    }

    handler_node->path         = path;
    handler_node->flags        = flags;
    handler_node->auto_release = false;

    // add the node to the list
    GG_LINKED_LIST_APPEND(&self->handlers, &handler_node->list_node);

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_Result
GG_CoapEndpoint_RegisterRequestHandler(GG_CoapEndpoint*       self,
                                       const char*            path,
                                       uint32_t               flags,
                                       GG_CoapRequestHandler* handler)
{
    GG_Result rc;

    GG_ASSERT(self);
    GG_ASSERT(path);
    GG_ASSERT(handler);
    GG_THREAD_GUARD_CHECK_BINDING(self);

    // skip any leading / character
    while (*path == '/') {
        ++path;
    }

    // allocate a handler node
    size_t path_length = strlen(path);
    size_t alloc_size = sizeof(GG_CoapRequestHandlerNode) + path_length + 1;
    GG_CoapRequestHandlerNode* handler_node =
        (GG_CoapRequestHandlerNode*)GG_AllocateZeroMemory(alloc_size);
    if (handler_node == NULL) {
        return GG_ERROR_OUT_OF_MEMORY;
    }

    // setup the node
    char* path_ptr = (char*)(handler_node + 1);
    memcpy(path_ptr, path, path_length + 1);

    handler_node->handler = handler;

    rc = GG_CoapEndpoint_RegisterRequestHandlerNode(self, path_ptr, flags, handler_node);

    handler_node->auto_release = true;

    return rc;
}

//----------------------------------------------------------------------
GG_Result
GG_CoapEndpoint_UnregisterRequestHandler(GG_CoapEndpoint*       self,
                                         const char*            path,
                                         GG_CoapRequestHandler* handler)
{
    GG_ASSERT(self);
    GG_THREAD_GUARD_CHECK_BINDING(self);

    // skip any leading / character
    while (path && *path == '/') {
        ++path;
    }

    GG_LINKED_LIST_FOREACH(node, &self->handlers) {
        GG_CoapRequestHandlerNode* handler_node  = GG_LINKED_LIST_ITEM(node, GG_CoapRequestHandlerNode, list_node);
        bool                       path_match    = !path || !strcmp(path, handler_node->path);
        bool                       handler_match = !handler || (handler_node->handler == handler);
        if (path_match && handler_match) {
            GG_LINKED_LIST_NODE_REMOVE(node);
            if (handler_node->auto_release) {
                GG_FreeMemory(handler_node);
            }
            return GG_SUCCESS;
        }
    }

    return GG_ERROR_NO_SUCH_ITEM;
}

//----------------------------------------------------------------------
GG_Result
GG_CoapEndpoint_SetDefaultRequestHandler(GG_CoapEndpoint* self, GG_CoapRequestHandler* handler)
{
    GG_ASSERT(self);
    GG_THREAD_GUARD_CHECK_BINDING(self);

    self->default_handler = handler;

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_Result
GG_CoapEndpoint_RegisterRequestFilterNode(GG_CoapEndpoint*          self,
                                          GG_CoapRequestFilterNode* filter_node)
{
    GG_ASSERT(self);
    GG_ASSERT(filter_node);
    GG_THREAD_GUARD_CHECK_BINDING(self);

    filter_node->auto_release = false;

    // add the node to the list
    GG_LINKED_LIST_APPEND(&self->request_filters, &filter_node->list_node);

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_Result
GG_CoapEndpoint_RegisterRequestFilter(GG_CoapEndpoint* self, GG_CoapRequestFilter* filter)
{
    GG_Result rc;

    GG_ASSERT(self);
    GG_ASSERT(filter);
    GG_THREAD_GUARD_CHECK_BINDING(self);

    // allocate a filter node
    GG_CoapRequestFilterNode* filter_node =
        (GG_CoapRequestFilterNode*)GG_AllocateMemory(sizeof(GG_CoapRequestFilterNode));
    if (filter_node == NULL) {
        return GG_ERROR_OUT_OF_MEMORY;
    }

    // setup the node
    filter_node->filter = filter;

    rc = GG_CoapEndpoint_RegisterRequestFilterNode(self, filter_node);

    filter_node->auto_release = true;

    return rc;
}

//----------------------------------------------------------------------
GG_Result
GG_CoapEndpoint_UnregisterRequestFilter(GG_CoapEndpoint* self, GG_CoapRequestFilter* filter)
{
    GG_ASSERT(self);
    GG_THREAD_GUARD_CHECK_BINDING(self);

    GG_LINKED_LIST_FOREACH(node, &self->request_filters) {
        GG_CoapRequestFilterNode* filter_node = GG_LINKED_LIST_ITEM(node, GG_CoapRequestFilterNode, list_node);
        if (filter_node->filter == filter) {
            GG_LINKED_LIST_NODE_REMOVE(node);
            if (filter_node->auto_release) {
                GG_FreeMemory(filter_node);
            }
            return GG_SUCCESS;
        }
    }

    return GG_ERROR_NO_SUCH_ITEM;
}

//----------------------------------------------------------------------
GG_Result
GG_CoapEndpoint_SetTokenPrefix(GG_CoapEndpoint* self, const uint8_t* prefix, size_t prefix_size)
{
    if (prefix_size > sizeof(self->token_prefix)) {
        return GG_ERROR_INVALID_PARAMETERS;
    }
    self->token_prefix_size = prefix_size;
    if (prefix_size) {
        GG_ASSERT(prefix);
        memcpy(self->token_prefix, prefix, prefix_size);
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
const uint8_t*
GG_CoapEndpoint_GetTokenPrefix(GG_CoapEndpoint* self, size_t* prefix_size)
{
    GG_ASSERT(prefix_size);
    *prefix_size = self->token_prefix_size;
    return self->token_prefix;
}

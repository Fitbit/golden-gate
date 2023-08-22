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
 * CoAP library public interface.
 *
 * Some useful terminology from RFC 7252 (https://tools.ietf.org/html/rfc7252)
 *
 * Endpoint
 *     An entity participating in the CoAP protocol.  Colloquially, an
 *     endpoint lives on a "Node", although "Host" would be more
 *     consistent with Internet standards usage, and is further
 *     identified by transport-layer multiplexing information that can
 *     include a UDP port number and a security association
 *
 *  Sender
 *     The originating endpoint of a message.  When the aspect of
 *     identification of the specific sender is in focus, also "source
 *     endpoint".
 *
 *  Recipient
 *     The destination endpoint of a message.  When the aspect of
 *     identification of the specific recipient is in focus, also
 *     "destination endpoint".
 *
 *  Client
 *     The originating endpoint of a request; the destination endpoint of
 *     a response.
 *
 *  Server
 *     The destination endpoint of a request; the originating endpoint of
 *     a response.
 */

 #pragma once

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include "xp/common/gg_io.h"
#include "xp/common/gg_inspect.h"
#include "xp/common/gg_lists.h"
#include "xp/common/gg_timer.h"
#include "xp/common/gg_types.h"

//! @addtogroup CoAP CoAP
//! CoAP client and server library
//! @{

#if defined(__cplusplus)
extern "C" {
#endif

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/

/**
 * CoAP message
 */
typedef struct GG_CoapMessage GG_CoapMessage;

/**
 * CoAP endpoint, used to send and receive messages
 */
typedef struct GG_CoapEndpoint GG_CoapEndpoint;

/**
 * CoAP async responder
 */
typedef struct GG_CoapResponder GG_CoapResponder;

/**
 * Handle used to reference a request without a direct pointer to it.
 * The special value GG_COAP_INVALID_REQUEST_HANDLE, equal to 0,
 * is a handle value that will never be assigned to a request by an endpoint,
 * so it can be used by clients as an initialization value that is known to never
 * be equal to an assigned handle.
 */
typedef uint64_t GG_CoapRequestHandle;

/**
 * Data type for a CoAP option value
 */
typedef enum {
    GG_COAP_MESSAGE_OPTION_TYPE_EMPTY,  ///< No data (used for empty options)
    GG_COAP_MESSAGE_OPTION_TYPE_UINT,   ///< The option data is an integer
    GG_COAP_MESSAGE_OPTION_TYPE_STRING, ///< The option data is a string
    GG_COAP_MESSAGE_OPTION_TYPE_OPAQUE  ///< The option data is an opaque byte array
} GG_CoapMessageOptionType;

/**
 * CoAP option.
 *
 * This structure can be used to represent options with different value types.
 * Depending on the `type` field, different union members of the `value` field
 * are used to represent the option value.
 */
typedef struct {
    uint32_t                 number; ///< Option number
    GG_CoapMessageOptionType type;   ///< Option value type
    /**
     * Option value
     */
    union {
        /**
         * Use this union member when the option data type is GG_COAP_MESSAGE_OPTION_TYPE_STRING
         */
        struct {
            const char*  chars;  ///< String characters, encoded as UTF-8
            size_t       length; ///< String length in bytes (may be set to 0 for null-terminated strings)
        } string;

        /**
         * Use this union member when the option data type is GG_COAP_MESSAGE_OPTION_TYPE_OPAQUE
         */
        struct {
            const uint8_t* bytes; ///< Byte array
            size_t         size;  ///< Size of the byte array
        } opaque;

        /**
         * Use this union member when the option data type is GG_COAP_MESSAGE_OPTION_TYPE_UINT
         */
        uint32_t uint; ///< Integer value
    } value;
} GG_CoapMessageOption;

/**
 * Struct used to pass array(s) of options when creating a CoAP message.
 *
 * This struct is intended to be used to hold on-stack initializers for
 * message options.
 */
typedef struct GG_CoapMessageOptionParam {
    GG_CoapMessageOption option;

    /**
     * This field allows options to be put in a linked list
     * It may be set to NULL when options are passed as an
     * array.
     */
    struct GG_CoapMessageOptionParam* next;

    /**
     * This field allows options to be sorted. It is used internally by the
     * CoAP runtime when serializing messages to datagrams, and should be
     * ignored by users of the API. It is part of this structure for efficiency,
     * so that the library does not have to allocate its own memory when sorting
     * options.
     */
    struct GG_CoapMessageOptionParam* sorted_next;
} GG_CoapMessageOptionParam;

/**
 * Iterator used to iterate through some or all the options in a CoAP message.
 * The iterator's `filter` field is used to decide whether all options will
 * be iterated over (filter == GG_COAP_MESSAGE_OPTION_ITERATOR_FILTER_ANY) or
 * only options with a specified `number` field (filter == <option number>).
 *
 * An iterator has reached the end of its iteration when the `option.number` field
 * of the iterator is `GG_COAP_MESSAGE_OPTION_NONE`
 */
typedef struct {
    GG_CoapMessageOption option;   ///< Current option for the iterator
    uint32_t             filter;   ///< Iterator filter
    const uint8_t*       location; ///< Internal field used by the implementation
    const uint8_t*       end;      ///< Internal field used by the implementation
} GG_CoapMessageOptionIterator;

/**
 * Interface implemented by listeners that want to be notified of CoAP responses.
 *
 * An `OnAck` always arrives before `OnResponse`.
 * If `OnError` is called, no other method will be called subsequently.
 */
GG_DECLARE_INTERFACE(GG_CoapResponseListener) {
    /**
     * Method called when an ACK is received
     *
     * @param self The object on which this method is invoked.
     */
    void (*OnAck)(GG_CoapResponseListener* self);

    /**
     * Method called when an error has occurred.
     *
     * @param self The object on which this method is invoked.
     * @param message Error message text (may be NULL).
     */
    void (*OnError)(GG_CoapResponseListener* self, GG_Result error, const char* message);

    /**
     * Method called when a response is received.
     *
     * > NOTE: the response object may not be used after this method returns, so
     * any data that must persist longer *must* be copied.
     *
     * @param self The object on which this method is invoked.
     * @param response The response that has been received.
     */
    void (*OnResponse)(GG_CoapResponseListener* self, GG_CoapMessage* response);
};

void GG_CoapResponseListener_OnAck(GG_CoapResponseListener* self);
void GG_CoapResponseListener_OnError(GG_CoapResponseListener* self, GG_Result error, const char* message);
void GG_CoapResponseListener_OnResponse(GG_CoapResponseListener* self, GG_CoapMessage* response);

typedef GG_Result GG_CoapRequestHandlerResult;

/**
 * Interface implemented by request handlers that may be registered with an endpoint.
 */
GG_DECLARE_INTERFACE(GG_CoapRequestHandler) {
    /**
     * Method invoked when a request has been received and should be handled by the handler.
     * The handler must either create a response message and return GG_SUCCESS, or return a
     * non-zero result and not create a response message.
     * If the handler returns GG_SUCCESS and a response message, that message is sent after
     * this function returns.
     * If the result is > 0 and <= 255, it is treated as a CoAP result code,
     * and a response with that code and an empty body will be generated and sent on behalf
     * of the handler.
     * If the result is GG_ERROR_WOULD_BLOCK, no response will be sent (the `responder` object
     * must be used subsequently to send the response asynchronously).
     * If the result is any other negative value, a response with code
     * GG_COAP_MESSAGE_CODE_INTERNAL_SERVER_ERROR and an empty body will be generated
     * and sent on behalf of the handler.
     *
     * NOTE: for asynchronous responses, the `request` object will remain valid until the
     * `responder` object is released.
     *
     * @param self The object on which this method is invoked.
     * @param endpoint The endpoint that received the request.
     * @param request The request that was received for this handler.
     * @param responder The responder object that may be used to respond asynchronously.
     * This parameter is NULL if the handler wasn't registered with the
     * GG_COAP_REQUEST_HANDLER_FLAG_ENABLE_ASYNC flag set.
     * @param transport_metadata Metadata associated with the transport from which the request
     * was received (typically of type GG_BUFFER_METADATA_TYPE_SOURCE_SOCKET_ADDRESS if the transport
     * is a UDP socket). May be NULL if no metadata exists for the request.
     * @param response Pointer to the variable where the handler should return its response.
     * The handler should only return a response in this variable if it also returns GG_SUCCESS.
     *
     * @return GG_SUCCESS if the handler was able to create a response object, or a CoAP result
     * code, or a negative error code.
     */
    GG_CoapRequestHandlerResult (*OnRequest)(GG_CoapRequestHandler*   self,
                                             GG_CoapEndpoint*         endpoint,
                                             const GG_CoapMessage*    request,
                                             GG_CoapResponder*        responder,
                                             const GG_BufferMetadata* transport_metadata,
                                             GG_CoapMessage**         response);
};

GG_Result GG_CoapRequestHandler_OnRequest(GG_CoapRequestHandler*   self,
                                          GG_CoapEndpoint*         endpoint,
                                          const GG_CoapMessage*    request,
                                          GG_CoapResponder*        responder,
                                          const GG_BufferMetadata* transport_metadata,
                                          GG_CoapMessage**         response);

/**
 * Entry in a list of handlers maintained by an endpoint.
 */
typedef struct {
    GG_LinkedListNode      list_node;       ///< Linked list node for storing handler node
    const char*            path;            ///< Path to register the under under
    uint32_t               flags;           ///< OR'ed combination of zero or mode CoAP flags
    bool                   auto_release;    ///< Flag indicating if this object should be freed
                                            ///< when handler is unregistered.
    GG_CoapRequestHandler* handler;         ///< CoAP request handler to be registered
} GG_CoapRequestHandlerNode;

/**
 * Interface implemented by request filters.
 *
 * Request filters may be registered with a CoAP endpoint. The CoAP endpoint iterates over all the
 * registered filters, sequentially, giving each filter an opportunity to inspect the request, as well
 * as the flags associated with the handler for the request, and either let the iteration continue on
 * to the next step, or cause it to terminate by providing a CoAP response or a CoAP result code from
 * which the CoAP endpoint can synthesize a CoAP response on its behalf.
 */
GG_DECLARE_INTERFACE(GG_CoapRequestFilter) {
    /**
     * Filter a request.
     * The filter may either:
     *   - terminate the filter chain by providing a response or a CoAP result code, in which
     *     case the CoAP endpoint will stop iterating over the filter chain, and respond immediately
     *     without invoking the registered handler for the request
     *  or
     *   - return GG_SUCCESS and not provide a response, in which case the CoAP endpoint will continue
     *     iterating over the filter chain to the next filter or the final handler
     *
     * @param self The object on which this method is invoked.
     * @param endpoint The endpoint that received the request.
     * @param handler_flags Flags associated with the handler for the request.
     * @param request The request to filter.
     * @param response Pointer to the variable where the filter may return a response if it terminates
     * the filter chain. The filter should only return a response in this variable if it also returns GG_SUCCESS.
     *
     * @return GG_SUCCESS or a CoAP result code if the filter was able to complete, or negative error code.
     */
    GG_CoapRequestHandlerResult (*FilterRequest)(GG_CoapRequestFilter* self,
                                                 GG_CoapEndpoint*      endpoint,
                                                 uint32_t              handler_flags,
                                                 const GG_CoapMessage* request,
                                                 GG_CoapMessage**      response);
};

//! @var GG_CoapRequestFilter::iface
//! Pointer to the virtual function table for the interface

//! @struct GG_CoapRequestFilterInterface
//! Virtual function table for the GG_CoapRequestFilter interface

//! @relates GG_CoapRequestFilter
//! @copydoc GG_CoapRequestFilterInterface::FilterRequest
GG_CoapRequestHandlerResult GG_CoapRequestFilter_FilterRequest(GG_CoapRequestFilter* self,
                                                               GG_CoapEndpoint*      endpoint,
                                                               uint32_t              handler_flags,
                                                               const GG_CoapMessage* request,
                                                               GG_CoapMessage**      response);

/**
 * Entry in a list of request filters maintained by an endpoint.
 */
typedef struct {
    GG_LinkedListNode     list_node;    ///< Linked list node for storing filter node
    GG_CoapRequestFilter* filter;       ///< CoAP request filter to be registered
    bool                  auto_release; ///< Flag indicating if this object should be freed
                                        ///< when filter is unregistered.
} GG_CoapRequestFilterNode;

/**
 * Parameters for custom CoAP client behavior/policy
 */
typedef struct {
    /*
     * Timeout after which a resend will happen, in milliseconds.
     * Set 0 to use a default value according to the CoAP specification and the
     * endpoint.
     */
    uint32_t ack_timeout;

    /*
     * Maximum number of times the client will resend the request if there is a response timeout.
     * For example, when set to 0, a request will only be sent once and not re-sent
     * if a response isn't received before the ack timeout (in which case the listener's
     * OnError handler will be invoked with GG_ERROR_TIMEOUT).
     */
    size_t max_resend_count;
} GG_CoapClientParameters;

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/

/**
 * CoAP request method
 *
 * > NOTE: CoAP encodes request methods and message codes in the same field, so
 * GG_CoapMethod value is read by calling `GG_CoapMessage_GetCode`
 */
typedef enum {
    GG_COAP_METHOD_GET = 1, ///< GET method
    GG_COAP_METHOD_POST,    ///< POST method
    GG_COAP_METHOD_PUT,     ///< PUT method
    GG_COAP_METHOD_DELETE   ///< DELETE method
} GG_CoapMethod;

/**
 * CoAP message type
 */
typedef enum {
    GG_COAP_MESSAGE_TYPE_CON, ///< CON-firmable message (will be ACK'ed)
    GG_COAP_MESSAGE_TYPE_NON, ///< NON-confirmable message (will not be ACK'ed)
    GG_COAP_MESSAGE_TYPE_ACK, ///< ACK message
    GG_COAP_MESSAGE_TYPE_RST  ///< ReSeT message
} GG_CoapMessageType;

#define GG_COAP_DEFAULT_PORT                       5683 ///< Default UDP port for unsecured CoAP
#define GG_COAP_DEFAULT_PORT_SECURE                5684 ///< Default UDP port for secured CoAP
#define GG_COAP_MESSGAGE_MAX_TOKEN_LENGTH          8    ///< Maximum lengnth of a message token
#define GG_COAP_MESSAGE_OPTION_ITERATOR_FILTER_ANY 0    ///< Indicates that an iterator iterates over all options

// request handler method filtering flags
#define GG_COAP_REQUEST_HANDLER_FLAG_ALLOW_GET     (1)       ///< Allow GET requests
#define GG_COAP_REQUEST_HANDLER_FLAG_ALLOW_POST    (1 << 1)  ///< Allow POST requests
#define GG_COAP_REQUEST_HANDLER_FLAG_ALLOW_PUT     (1 << 2)  ///< Allow PUT requests
#define GG_COAP_REQUEST_HANDLER_FLAG_ALLOW_DELETE  (1 << 3)  ///< Allow DELETE requests
#define GG_COAP_REQUEST_HANDLER_FLAGS_ALLOW_ALL    (0xF)     ///< Allow all requests

// request handler feature flags
#define GG_COAP_REQUEST_HANDLER_FLAG_ENABLE_ASYNC  (1 << 4)  ///< Enable async responses from the handler

// request handler group flags
// (4 groups are defined, a handler can be in any combination of the groups, in addition to the
// virtual group 0, which all handlers are implicitly always a member of)
// - the meaning of group membership is not specified here, it may be defined elsewhere -
#define GG_COAP_REQUEST_HANDLER_FLAG_GROUP(group)  (1 << (23 + (group)))
#define GG_COAP_REQUEST_HANDLER_FLAG_GROUP_1       GG_COAP_REQUEST_HANDLER_FLAG_GROUP(1) ///< Belongs to group 1
#define GG_COAP_REQUEST_HANDLER_FLAG_GROUP_2       GG_COAP_REQUEST_HANDLER_FLAG_GROUP(2) ///< Belongs to group 2
#define GG_COAP_REQUEST_HANDLER_FLAG_GROUP_3       GG_COAP_REQUEST_HANDLER_FLAG_GROUP(3) ///< Belongs to group 3
#define GG_COAP_REQUEST_HANDLER_FLAG_GROUP_4       GG_COAP_REQUEST_HANDLER_FLAG_GROUP(4) ///< Belongs to group 4

// message codes from RFC7252
#define GG_COAP_MESSAGE_CODE_CLASS(c)  (((c) >> 5) & 7)
#define GG_COAP_MESSAGE_CODE_DETAIL(c) ((c) & 0x1F)
#define GG_COAP_MESSAGE_CODE(x)        ((((x) / 100) << 5) | ((x) % 100))
#define GG_COAP_MESSAGE_CODE_CREATED                      GG_COAP_MESSAGE_CODE(201)
#define GG_COAP_MESSAGE_CODE_DELETED                      GG_COAP_MESSAGE_CODE(202)
#define GG_COAP_MESSAGE_CODE_VALID                        GG_COAP_MESSAGE_CODE(203)
#define GG_COAP_MESSAGE_CODE_CHANGED                      GG_COAP_MESSAGE_CODE(204)
#define GG_COAP_MESSAGE_CODE_CONTENT                      GG_COAP_MESSAGE_CODE(205)
#define GG_COAP_MESSAGE_CODE_CONTINUE                     GG_COAP_MESSAGE_CODE(231)
#define GG_COAP_MESSAGE_CODE_BAD_REQUEST                  GG_COAP_MESSAGE_CODE(400)
#define GG_COAP_MESSAGE_CODE_UNAUTHORIZED                 GG_COAP_MESSAGE_CODE(401)
#define GG_COAP_MESSAGE_CODE_BAD_OPTION                   GG_COAP_MESSAGE_CODE(402)
#define GG_COAP_MESSAGE_CODE_FORBIDDEN                    GG_COAP_MESSAGE_CODE(403)
#define GG_COAP_MESSAGE_CODE_NOT_FOUND                    GG_COAP_MESSAGE_CODE(404)
#define GG_COAP_MESSAGE_CODE_METHOD_NOT_ALLOWED           GG_COAP_MESSAGE_CODE(405)
#define GG_COAP_MESSAGE_CODE_NOT_ACCEPTABLE               GG_COAP_MESSAGE_CODE(406)
#define GG_COAP_MESSAGE_CODE_REQUEST_ENTITY_INCOMPLETE    GG_COAP_MESSAGE_CODE(408)
#define GG_COAP_MESSAGE_CODE_PRECONDITION_FAILED          GG_COAP_MESSAGE_CODE(412)
#define GG_COAP_MESSAGE_CODE_REQUEST_ENTITY_TOO_LARGE     GG_COAP_MESSAGE_CODE(413)
#define GG_COAP_MESSAGE_CODE_UNSUPPORTED_CONTENT_FORMAT   GG_COAP_MESSAGE_CODE(415)
#define GG_COAP_MESSAGE_CODE_INTERNAL_SERVER_ERROR        GG_COAP_MESSAGE_CODE(500)
#define GG_COAP_MESSAGE_CODE_NOT_IMPLEMENTED              GG_COAP_MESSAGE_CODE(501)
#define GG_COAP_MESSAGE_CODE_BAD_GATEWAY                  GG_COAP_MESSAGE_CODE(502)
#define GG_COAP_MESSAGE_CODE_SERVICE_UNAVAILABLE          GG_COAP_MESSAGE_CODE(503)
#define GG_COAP_MESSAGE_CODE_GATEWAY_TIMEOUT              GG_COAP_MESSAGE_CODE(504)
#define GG_COAP_MESSAGE_CODE_PROXYING_NOT_SUPPORTED       GG_COAP_MESSAGE_CODE(505)

// code classes
// the class can indicate:
//   a request (0)
//   a success response (2)
//   a client error response (4)
//   a server error response (5)
#define GG_COAP_MESSAGE_CODE_CLASS_REQUEST                  0
#define GG_COAP_MESSAGE_CODE_CLASS_SUCCESS_RESPONSE         2
#define GG_COAP_MESSAGE_CODE_CLASS_CLIENT_ERROR_RESPONSE    4
#define GG_COAP_MESSAGE_CODE_CLASS_SERVER_ERROR_RESPONSE    5

// option numbers
#define GG_COAP_MESSAGE_OPTION_NONE           0 ///< Not a real option number, only used as a sentinel
#define GG_COAP_MESSAGE_OPTION_IF_MATCH       1
#define GG_COAP_MESSAGE_OPTION_URI_HOST       3
#define GG_COAP_MESSAGE_OPTION_ETAG           4
#define GG_COAP_MESSAGE_OPTION_IF_NONE_MATCH  5
#define GG_COAP_MESSAGE_OPTION_URI_PORT       7
#define GG_COAP_MESSAGE_OPTION_LOCATION_PATH  8
#define GG_COAP_MESSAGE_OPTION_URI_PATH       11
#define GG_COAP_MESSAGE_OPTION_CONTENT_FORMAT 12
#define GG_COAP_MESSAGE_OPTION_MAX_AGE        14
#define GG_COAP_MESSAGE_OPTION_URI_QUERY      15
#define GG_COAP_MESSAGE_OPTION_ACCEPT         17
#define GG_COAP_MESSAGE_OPTION_LOCATION_QUERY 20
#define GG_COAP_MESSAGE_OPTION_PROXY_URI      35
#define GG_COAP_MESSAGE_OPTION_PROXY_SCHEME   39
#define GG_COAP_MESSAGE_OPTION_SIZE1          60
#define GG_COAP_MESSAGE_OPTION_SIZE2          28
#define GG_COAP_MESSAGE_OPTION_BLOCK1         27
#define GG_COAP_MESSAGE_OPTION_BLOCK2         23
#define GG_COAP_MESSAGE_OPTION_START_OFFSET   2048 ///< vendor-specific option number
#define GG_COAP_MESSAGE_OPTION_EXTENDED_ERROR 2049 ///< vendor-specific extended error code option number

// formats IDs
#define GG_COAP_MESSAGE_FORMAT_ID_TEXT_PLAIN    0
#define GG_COAP_MESSAGE_FORMAT_ID_LINK_FORMAT   40
#define GG_COAP_MESSAGE_FORMAT_ID_XML           41
#define GG_COAP_MESSAGE_FORMAT_ID_OCTET_STREAM  42
#define GG_COAP_MESSAGE_FORMAT_ID_EXI           47
#define GG_COAP_MESSAGE_FORMAT_ID_JSON          50
#define GG_COAP_MESSAGE_FORMAT_ID_CBOR          60

// error codes
#define GG_ERROR_COAP_UNSUPPORTED_VERSION (GG_ERROR_BASE_COAP - 0)
#define GG_ERROR_COAP_RESET               (GG_ERROR_BASE_COAP - 1)
#define GG_ERROR_COAP_UNEXPECTED_MESSAGE  (GG_ERROR_BASE_COAP - 2)
#define GG_ERROR_COAP_SEND_FAILURE        (GG_ERROR_BASE_COAP - 3)
#define GG_ERROR_COAP_UNEXPECTED_BLOCK    (GG_ERROR_BASE_COAP - 4)
#define GG_ERROR_COAP_INVALID_RESPONSE    (GG_ERROR_BASE_COAP - 5)
#define GG_ERROR_COAP_ETAG_MISMATCH       (GG_ERROR_BASE_COAP - 6)

// options-related constants
#define GG_COAP_MESSAGE_MAX_OPTION_COUNT     1024          ///< Sanity-check bound for number of options in a message
#define GG_COAP_MESSAGE_MAX_OPTION_SIZE      (269 + 65536) ///< Maximum possible size of an option payload
#define GG_COAP_MESSAGE_MAX_ETAG_OPTION_SIZE 8             ///< Maximum size of an ETag option

// custom option related constants
#define GG_COAP_EXTENDED_ERROR_OPTION_SIZE  8

#define GG_COAP_DEFAULT_MAX_RETRANSMIT      4 ///< Maximum number of retransmissions, by default

// request handle value that is guaranteed to never be used by an endpoint
#define GG_COAP_INVALID_REQUEST_HANDLE      0

/*----------------------------------------------------------------------
|   macros
+---------------------------------------------------------------------*/
#define GG_COAP_MESSAGE_OPTION_PARAM_EMPTY_N(_number) \
    {{ (_number), GG_COAP_MESSAGE_OPTION_TYPE_EMPTY, { .uint = 0 }}, NULL, NULL}
/**
 * Initializer for an GG_COAP_MESSAGE_OPTION_TYPE_EMPTY option parameter.
 * Example: GG_COAP_MESSAGE_OPTION_PARAM_EMPTY(IF_NONE_MATCH)
 * @param _option Name of the option.
 */
#define GG_COAP_MESSAGE_OPTION_PARAM_EMPTY(_option) \
    GG_COAP_MESSAGE_OPTION_PARAM_EMPTY_N(GG_COAP_MESSAGE_OPTION_ ## _option)

#define GG_COAP_MESSAGE_OPTION_PARAM_UINT_N(_number, _uint) \
    {{ (_number), GG_COAP_MESSAGE_OPTION_TYPE_UINT, { .uint = (_uint) }}, NULL, NULL}

/**
 * Initializer for an GG_COAP_MESSAGE_OPTION_TYPE_UINT option parameter.
 * Example: GG_COAP_MESSAGE_OPTION_PARAM_UINT(URI_PORT, 5683)
 *
 * @param _option Name of the option.
 * @param _uint Integer value.
 */
#define GG_COAP_MESSAGE_OPTION_PARAM_UINT(_option, _uint) \
    GG_COAP_MESSAGE_OPTION_PARAM_UINT_N(GG_COAP_MESSAGE_OPTION_ ## _option, (_uint))

#define GG_COAP_MESSAGE_OPTION_PARAM_STRING_NL(_number, _string, _length) \
    {{ (_number), GG_COAP_MESSAGE_OPTION_TYPE_STRING, { .string = { (_string), (_length)}}}, NULL, NULL}

/**
 * Initializer for a #GG_COAP_MESSAGE_OPTION_TYPE_STRING option parameter, specifying the length.
 * Example: GG_COAP_MESSAGE_OPTION_PARAM_STRING_L(URI_PATH, "hello", 5)
 *
 * @param _option Name of the option.
 * @param _string String value (doesn't need to be null-terminated).
 * @param _length Size of the string.
 */
#define GG_COAP_MESSAGE_OPTION_PARAM_STRING_L(_option, _string, _length) \
    GG_COAP_MESSAGE_OPTION_PARAM_STRING_NL(GG_COAP_MESSAGE_OPTION_ ## _option, (_string), (_length))

#define GG_COAP_MESSAGE_OPTION_PARAM_STRING_N(_number, _string) \
    GG_COAP_MESSAGE_OPTION_PARAM_STRING_NL((_number), (_string), 0)

/**
 * Initializer for a #GG_COAP_MESSAGE_OPTION_TYPE_STRING option parameter, with a null-terminated string.
 * Example: GG_COAP_MESSAGE_OPTION_PARAM_STRING(URI_PATH, "hello")
 *
 * @param _option Name of the option.
 * @param _string String value (doesn't need to be null-terminated).
 */
#define GG_COAP_MESSAGE_OPTION_PARAM_STRING(_option, _string) \
    GG_COAP_MESSAGE_OPTION_PARAM_STRING_NL(GG_COAP_MESSAGE_OPTION_ ## _option, (_string), 0)

#define GG_COAP_MESSAGE_OPTION_PARAM_OPAQUE_N(_number, _bytes, _size) \
    {{ (_number), GG_COAP_MESSAGE_OPTION_TYPE_OPAQUE, { .opaque = { (_bytes), (_size) }}}, NULL, NULL}

/**
 * Initializer for a #GG_COAP_MESSAGE_OPTION_TYPE_OPAQUE option parameter.
 * Example: GG_COAP_MESSAGE_OPTION_PARAM_OPAQUE(ETAG, etag_byte_array, sizeof(etag_byte_array))
 *
 * @param _option Name of the option.
 * @param _bytes Byte array.
 * @param _size Size of the byte array.
 */
#define GG_COAP_MESSAGE_OPTION_PARAM_OPAQUE(_option, _bytes, _size) \
    GG_COAP_MESSAGE_OPTION_PARAM_OPAQUE_N(GG_COAP_MESSAGE_OPTION_ ## _option, (_bytes), (_size))

/**
 * Stepping macro to follow option params linkage, allowing for next-pointer and array-layout traversal.
 */
#define GG_COAP_MESSAGE_OPTION_PARAM_NEXT(_option)\
((_option)->next ? (_option)->next : (_option) + 1)

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/
/**
 * Create a CoAP message object.
 *
 * A message may be created with no payload (`payload` == NULL and `payload_size` == 0),
 * a payload that is already known (`payload` != NULL and `payload_size` != 0), or
 * a payload reservation (`payload` == NULL, and `payload_size` != 0). When using a reservation,
 * the payload will initially be filled with zeros. It may be updated subsequently by obtaining
 * a pointer to the payload buffer by calling `GG_CoapMessage_UsePayload` and writing new data
 * in the buffer, prior to sending the message.
 *
 * > NOTE: the payload data passed to this function is copied, so the caller does not need to
 * keep that memory valid after the call returns.
 *
 * The `options` passed to this constructor are contained in possibly-chained arrays of
 * #GG_CoapMessageOptionParam structs. Each struct should be initialized using one of:
 *   - #GG_COAP_MESSAGE_OPTION_PARAM_EMPTY
 *   - #GG_COAP_MESSAGE_OPTION_PARAM_UINT
 *   - #GG_COAP_MESSAGE_OPTION_PARAM_STRING
 *   - #GG_COAP_MESSAGE_OPTION_PARAM_STRING_L
 *   - #GG_COAP_MESSAGE_OPTION_PARAM_OPAQUE
 *
 * When reading the `options` used to initialize the message, the constructor will read
 * `options_count` options, as follows:
 * The first option is pointed to by the `options` parameter. For each subsequent options, up
 * to `options_count`, the option pointer is obtained either by following the `next` pointer if
 * not NULL, or by iterating to the next pointer assuming an array layout.
 * Chaining through the `next` pointer must be done by the caller before calling this constructor.
 *
 * > NOTE: the implementation of this method uses the `next` field of options params internally
 * to sort the options when serializing, so the caller should expect those fields to be different
 * from them original values when the method returns.
 *
 * @param code Message code (use a constant from #GG_CoapMethod or `GG_COAP_MESSAGE_CODE_xxx`)
 * @param type Message type.
 * @param options Array of message options (or NULL if there are no options).
 * @param options_count Number of options.
 * @param message_id Message ID.
 * @param token Message token.
 * @param token_length Length of the message token.
 * @param payload Message payload (may be NULL if the payload size is 0, or the payload is unspecified).
 * @param payload_size Size of the payload.
 * @param message Pointer to the variable where the message object will be returned.
 *
 * @return GG_SUCCESS if the message could be created, or a negative error code.
 */
GG_Result GG_CoapMessage_Create(uint8_t                    code,
                                GG_CoapMessageType         type,
                                GG_CoapMessageOptionParam* options,
                                size_t                     options_count,
                                uint16_t                   message_id,
                                const uint8_t*             token,
                                size_t                     token_length,
                                const uint8_t*             payload,
                                size_t                     payload_size,
                                GG_CoapMessage**           message);

/**
 * Destroy a CoAP message.
 *
 * @param self The object on which this method is called.
 */
void GG_CoapMessage_Destroy(GG_CoapMessage* self);

/**
 * Create a CoAP message by parsing an encoded datagram.
 *
 * @param datagram The datagram to parse.
 * @param message Pointer to the variable where the message object will be returned.
 *
 * @return GG_SUCCESS if the message could be created, or a negative error code.
 */
GG_Result GG_CoapMessage_CreateFromDatagram(GG_Buffer* datagram, GG_CoapMessage** message);

/**
 * Obtain a #GG_Buffer buffer containing the encoded datagram representation of a CoAP message.
 * @param self The object on which this method is called.
 * @param datagram Pointer to the variable where the datagram will be returned.
 *
 * @return GG_SUCCESS if the call succeeded, or a negative error code.
 */
GG_Result GG_CoapMessage_ToDatagram(const GG_CoapMessage* self, GG_Buffer** datagram);

/**
 * Obtain the GG_Inspectable interface for a CoAP message.
 *
 */
GG_Inspectable* GG_CoapMessage_AsInspectable(GG_CoapMessage* self);

/**
 * Get the type of a CoAP message.
 *
 * @param self The object on which this method is called.
 *
 * @return The message type.
 */
GG_CoapMessageType GG_CoapMessage_GetType(const GG_CoapMessage* self);

/**
 * Get the code of a CoAP message.
 *
 * > NOTE a message code in the range 0-100 is also called a request method
 * (values from the #GG_CoapMethod enumeration).
 * Message codes equal to or greater to 100 have constant macros named
 * GG_COAP_MESSAGE_CODE_xxx
 *
 * @param self The object on which this method is called.
 *
 * @return The message code.
 */
uint8_t GG_CoapMessage_GetCode(const GG_CoapMessage* self);

/**
 * Get the token of a CoAP message.
 *
 * @param self The object on which this method is called.
 * @param token Buffer in which the token bytes will be returned. This buffer must be able to
 * hold at least #GG_COAP_MESSGAGE_MAX_TOKEN_LENGTH bytes.
 *
 *
 * @return The size of the token.
 */
size_t GG_CoapMessage_GetToken(const GG_CoapMessage* self, uint8_t* token);

/**
 * Get the ID of a CoAP message.
 *
 * @param self The object on which this method is called.
 *
 * @return The message ID.
 */
uint16_t GG_CoapMessage_GetMessageId(const GG_CoapMessage* self);

/**
 * Get the size of a CoAP message payload.
 *
 * @param self The object on which this method is called.
 *
 * @return The payload size.
 */
size_t GG_CoapMessage_GetPayloadSize(const GG_CoapMessage* self);

/**
 * Get the payload of a CoAP message.
 *
 * @param self The object on which this method is called.
 *
 * @return The payload bytes, or NULL if the payload size is 0.
 */
const uint8_t* GG_CoapMessage_GetPayload(const GG_CoapMessage* self);

/**
 * Get the writeable payload buffer of a CoAP message.
 *
 * @param self The object on which this method is called.
 *
 * @return The payload buffer, or NULL if the payload size is 0 or the payload is read-only.
 */
uint8_t* GG_CoapMessage_UsePayload(const GG_CoapMessage* self);

/**
 * Get a message option by number (and optional index)
 *
 * NOTE: this will potentially scan all options in the message, so if
 * repeated calls to this method are made, it may be worth considering using
 * an option iterator instead.
 * @see GG_CoapMessageOptionIterator
 *
 * @param self The object on which this method is called.
 * @param option_number Number of the option to get (see GG_COAP_MESSAGE_OPTION_XXX constants).
 * @param option Pointer to the struct in which the option will be returned.
 * @param index Zero-based index of the option to get (out of the list of
 * options of the specified type, not of all options). Pass 0 for the first
 * matching option.
 *
 * @return GG_SUCCESS if the option was found, or a negative error code.
 */
GG_Result GG_CoapMessage_GetOption(const GG_CoapMessage* self,
                                   uint32_t              option_number,
                                   GG_CoapMessageOption* option,
                                   unsigned int          index);

/**
 * Initialize a option iterator for a CoAP message.
 * (See the #GG_CoapMessageOptionIterator documentation for details on
 * how message option iterators work).
 * @see GG_CoapMessageOptionIterator
 *
 * @param self The object on which this method is called.
 * @param filter The filter indicating which options to include in the iteration.
 * @param iterator Pointer to an iterator struct to initialize.
 */
void GG_CoapMessage_InitOptionIterator(const GG_CoapMessage*         self,
                                       uint32_t                      filter,
                                       GG_CoapMessageOptionIterator* iterator);

/**
 * Advance an option iterator for a CoAP message by one step.
 * (See the #GG_CoapMessageOptionIterator documentation for details on
 * how message option iterators work).
 *
 * @param self The object on which this method is called.
 * @param iterator The iterator to advance.
 */
void GG_CoapMessage_StepOptionIterator(const GG_CoapMessage*         self,
                                       GG_CoapMessageOptionIterator* iterator);

/**
 * Create a new CoAP endpoint object.
 *
 * NOTE: the connection_sink and connection_source parameters may be NULL when
 * calling this constructor. In that case, the connection source and sinks may
 * be set later by using the endpoint's GG_DataSource and GG_DataSink interfaces.
 *
 * @param timer_scheduler Timer scheduler that will be used by the endpoint to create timers.
 * @param connection_sink Data sink to which datagrams will be sent (may be NULL).
 * @param connection_source Data source from which datagrams will be received (may be NULL).
 * NOTE: it is the responsibility of the caller to disconnect that source from the endpoint
 * before destroying the endpoint.
 * @param endpoint Pointer to the variable where the object will be returned.
 *
 * @return GG_SUCCESS if the object could be created, or a negative error code.
 */
GG_Result GG_CoapEndpoint_Create(GG_TimerScheduler* timer_scheduler,
                                 GG_DataSink*       connection_sink,
                                 GG_DataSource*     connection_source,
                                 GG_CoapEndpoint**  endpoint);

/**
 * Destroy a CoAP endpoint.
 *
 * @param self The object on which this method is called.
 */
void GG_CoapEndpoint_Destroy(GG_CoapEndpoint* self);

/**
 * Obtain the #GG_DataSink interface for an endpoint.
 * This data sink may be used to send datagrams to the endpoint.
 *
 * @param self The object on which this method is called.
 * @return The endpoint's GG_DataSink interface.
 */
GG_DataSink* GG_CoapEndpoint_AsDataSink(GG_CoapEndpoint* self);

/**
 * Obtain the #GG_DataSource interface for an endpoint.
 * This data sink may be used to receive datagrams from the endpoint.
 *
 * @param self The object on which this method is called.
 * @return The endpoint's GG_DataSource interface.
 */
GG_DataSource* GG_CoapEndpoint_AsDataSource(GG_CoapEndpoint* self);

/**
 * Obtain the #GG_Inspectable interface for an endpoint.
 *
 * @param self The object on which this method is called.
 * @return The endpoint's GG_Inspectable interface.
 */
GG_Inspectable* GG_CoapEndpoint_AsInspectable(GG_CoapEndpoint* self);

/**
 * Send a CoAP request.
 * (see the documentation for GG_CoapMessage_Create() for details on
 * how to pass options and payload.)
 *
 * @param self The object on which this method is called.
 * @param method Method for the request.
 * @param options Options for the request.
 * @param options_count Number of options for the request.
 * @param payload Payload for the request.
 * @param payload_size Size of the payload.
 * @param client_parameters Optional client parameters to customize the client behavior. Pass NULL for defaults.
 * @param listener Listener object that will receive callbacks regarding any response or error.
 * @param request_handle Handle to the request, that may be used subsequently to cancel the request.
 * (the caller may pass NULL if it is not interested in the handle value)
 *
 * @return GG_SUCCESS if the call succeeded, or a negative error code.
 */
GG_Result GG_CoapEndpoint_SendRequest(GG_CoapEndpoint*               self,
                                      GG_CoapMethod                  method,
                                      GG_CoapMessageOptionParam*     options,
                                      size_t                         options_count,
                                      const uint8_t*                 payload,
                                      size_t                         payload_size,
                                      const GG_CoapClientParameters* client_parameters,
                                      GG_CoapResponseListener*       listener,
                                      GG_CoapRequestHandle*          request_handle);

/**
 * Send a CoAP request with a buffer source.
 * This method is similar to GG_CoapEndpoint_SendRequest, but with the payload passed as a
 * GG_BufferSource rather than a fixed buffer.
 *
 * @param self The object on which this method is called.
 * @param method Method for the request.
 * @param options Options for the request.
 * @param options_count Number of options for the request.
 * @param payload_source Payload source for the request.
 * @param client_parameters Optional client parameters to customize the client behavior. Pass NULL for defaults.
 * @param listener Listener object that will receive callbacks regarding any response or error.
 * @param request_handle Handle to the request, that may be used subsequently to cancel the request.
 *
 * @return GG_SUCCESS if the call succeeded, or a negative error code.
 */
GG_Result GG_CoapEndpoint_SendRequestFromBufferSource(GG_CoapEndpoint*               self,
                                                      GG_CoapMethod                  method,
                                                      GG_CoapMessageOptionParam*     options,
                                                      size_t                         options_count,
                                                      GG_BufferSource*               payload_source,
                                                      const GG_CoapClientParameters* client_parameters,
                                                      GG_CoapResponseListener*       listener,
                                                      GG_CoapRequestHandle*          request_handle);

/**
 * Create a CoAP response.
 * (see the documentation for GG_CoapMessage_Create() for details on
 * how to pass options and payload.)
 *
 * @param self The object on which this method is called.
 * @param request The request for which the response is created.
 * @param code The response code.
 * @param options Options for the response.
 * @param options_count Number of options for the response.
 * @param payload Payload for the response.
 * @param payload_size Size of the payload.
 * @param response Pointer to the variable in which the object will be returned.
 *
 * @return GG_SUCCESS if the object could be created, or a negative error code.
 */
GG_Result GG_CoapEndpoint_CreateResponse(GG_CoapEndpoint*           self,
                                         const GG_CoapMessage*      request,
                                         uint8_t                    code,
                                         GG_CoapMessageOptionParam* options,
                                         size_t                     options_count,
                                         const uint8_t*             payload,
                                         size_t                     payload_size,
                                         GG_CoapMessage**           response);

/**
 * Cancel a previously sent request.
 * When a request is cancelled, its listener will not be called, even if a response datagram is
 * received.
 *
 * @param self The object on which this method is called.
 * @param request Handle of the request to cancel.
 *
 * @return GG_SUCCESS if the call succeeded, or a negative error code.
 */
GG_Result GG_CoapEndpoint_CancelRequest(GG_CoapEndpoint* self, GG_CoapRequestHandle request);

/**
 * Register a handler to be called when a request is received for a certain path.
 * The path under which a handler is registered consists of one or more path components,
 * separated by '/' characters. No leading or trailing '/' characters should appear in the
 * path.
 * Matching of incoming requests against registered handler paths is performed as follows:
 * The handlers are checked one by one in the order in which they were registered. The first
 * matching handler will be invoked to handle the request. To match, all the URI path components
 * of the request must match the '/'-separated components of the handler's path. Partial matches
 * are allowed. For example, a handler registered at path `foo/bar/baz` will match a request with
 * the URI path components (`foo`, `bar`, `baz`) but also a request with just (`foo`, `bar`).
 *
 * NOTE: this method makes an internal copy of the path parameter.
 *
 * @param self The object on which this method is called.
 * @param path Path to register the handler under.
 * @param flags OR'ed combination of zero or more flags.
 * @param handler The handler object to be registered.
 * See GG_COAP_REQUEST_HANDLER_FLAG_XXX constants.
 *
 * @return GG_SUCCESS if the call succeeded, or a negative error code.
 */
GG_Result GG_CoapEndpoint_RegisterRequestHandler(GG_CoapEndpoint*       self,
                                                 const char*            path,
                                                 uint32_t               flags,
                                                 GG_CoapRequestHandler* handler);
/**
 * Register a handler stored in a handler node to be called when a request is received
 * for a certain path.
 *
 * This method is a variant of GG_CoapEndpoint_RegisterRequestHandler which doesn't do any
 * dynamic memory allocation.
 *
 * NOTE: this method doesn't make an internal copy of the path parameter, so it must remain
 * unchanged for as long as the handler is registered.
 * NOTE: only handler_node->handler should be filled in, as the other fields will be overwritten
 * by this method.
 *
 * @param self The object on which this method is called.
 * @param path Path to register the handler under.
 * @param flags OR'ed combination of zero or more flags.
 * @param handler_node The handler node object which contains handler to be registered.
 * See GG_COAP_REQUEST_HANDLER_FLAG_XXX constants.
 *
 * @return GG_SUCCESS if the call succeeded, or a negative error code.
 */
GG_Result GG_CoapEndpoint_RegisterRequestHandlerNode(GG_CoapEndpoint*           self,
                                                     const char*                path,
                                                     uint32_t                   flags,
                                                     GG_CoapRequestHandlerNode* handler_node);

/**
 * Unregister a previously registered handler.
 * Only the first matching combination of path and/or handler will be unregistered, if found.
 *
 * @param self The object on which this method is called.
 * @param path The path at which the handler was registered, or NULL to unregister the first matching
 * handler regardless of the path.
 * @param handler The handler to unregister, or NULL to unregister any handler at the specified path.
 *
 * @return GG_SUCCESS if the call succeeded, or a negative error code.
 */
GG_Result GG_CoapEndpoint_UnregisterRequestHandler(GG_CoapEndpoint*       self,
                                                   const char*            path,
                                                   GG_CoapRequestHandler* handler);

/**
 * Set the default handler.
 * The default handler, if set, is invoked when a request is received and no registered
 * handler matches.
 *
 * @param self The object on which this method is called.
 * @param handler The handler that should be used as the default handler.
 *
 * @return GG_SUCCESS if the call succeeded, or a negative error code.
 */
GG_Result GG_CoapEndpoint_SetDefaultRequestHandler(GG_CoapEndpoint* self, GG_CoapRequestHandler* handler);

/**
 * Register a request filter.
 * Request filters are invoked sequentially, in the order they were registered, until they have
 * all been invoked or one of them has responded in a way that terminates the filter chain iteration.
 * (See GG_CoapRequestFilter::FilterRequest for details).
 *
 * @param self The object on which this method is called.
 * @param filter The filter to register.
 *
 * @return GG_SUCCESS if the call succeeded, or a negative error code.
 */
GG_Result GG_CoapEndpoint_RegisterRequestFilter(GG_CoapEndpoint* self, GG_CoapRequestFilter* filter);

/**
 * Register a request filter stored in a filter node.
 *
 * This method is a variant of GG_CoapEndpoint_RegisterRequestFilter that doesn do any
 * dynamic memory allocation.
 *
 * NOTE: Only filter_node->filter should be filled in, as the other fields will be overwritten
 * by this method.
 *
 * @param self The object on which this method is called.
 * @param filter_node The filter node containing the filter to register.
 *
 * @return GG_SUCCESS if the call succeeded, or a negative error code.
 */
GG_Result GG_CoapEndpoint_RegisterRequestFilterNode(GG_CoapEndpoint* self,
                                                    GG_CoapRequestFilterNode* filter_node);

/**
 * Unregister a request filter.
 *
 * @param self The object on which this method is called.
 * @param filter The filter to unregister.
 *
 * @return GG_SUCCESS if the call succeeded, or a negative error code.
 */
GG_Result GG_CoapEndpoint_UnregisterRequestFilter(GG_CoapEndpoint* self, GG_CoapRequestFilter* filter);

/**
 * Set the token prefix.
 * Up to 4 bytes of prefix can be added to each message token. This is an advanced feature that
 * may be useful for very specific conditions where tokens need to be differentiated between
 * different endpoints, in a coordinated fashion.
 *
 * @param self The object on which this method is called.
 * @param prefix Prefix bytes.
 * @param prefix_size Size of the prefix (1 to 4, or 0 to disable).
 *
 * @return GG_SUCCESS if the call succeeded, or a negative error code.
 */
GG_Result GG_CoapEndpoint_SetTokenPrefix(GG_CoapEndpoint* self, const uint8_t* prefix, size_t prefix_size);

/**
 * Get the token prefix.
 *
 * @param self The object on which this method is called.
 * @param prefix_size Address of the variable where the prefix size will be returned.
 *
 * @return Pointer to the prefix bytes.
 */
const uint8_t* GG_CoapEndpoint_GetTokenPrefix(GG_CoapEndpoint* self, size_t* prefix_size);

/**
 * Create a CoAP response.
 * This is essentially the same as `GG_CoapEndpoint_CreateResponse` but using the endpoint
 * and request references held by the responder.
 * This method would be typically used instead of the simpler GG_CoapResponder_Respond in
 * cases where the payload of the response needs to be updated after the response is created,
 * prior to sending it (for example when the payload is dynamically generated and the caller
 * wishes to avoid allocating storage for it other than the final response itself.)
 *
 * @param self The object on which this method is called.
 * @param code The response code.
 * @param options Options for the response.
 * @param options_count Number of options for the response.
 * @param payload Payload for the response.
 * @param payload_size Size of the payload.
 * @param response Pointer to the variable in which the object will be returned.
 *
 * @return GG_SUCCESS if the object could be created, or a negative error code.
 */
GG_Result GG_CoapResponder_CreateResponse(GG_CoapResponder*          self,
                                          uint8_t                    code,
                                          GG_CoapMessageOptionParam* options,
                                          size_t                     options_count,
                                          const uint8_t*             payload,
                                          size_t                     payload_size,
                                          GG_CoapMessage**           response);

/**
 * Send a response using a responder
 *
 * @param self The object on which this method is called.
 * @param response The response to send.
 *
 * @return GG_SUCCESS if the method succeeds, or a negative error code.
 */
GG_Result GG_CoapResponder_SendResponse(GG_CoapResponder* self, GG_CoapMessage* response);

/**
 * Create and send a response.
 * This is a convenience method that is equivalent to calling GG_CoapResponder_CreateResponse
 * to create a response, followed by GG_CoapResponder_SendResponse to send that response.
 *
 * @param self The object on which this method is called.
 * @param code The response code.
 * @param options Options for the response.
 * @param options_count Number of options for the response.
 * @param payload Payload for the response.
 * @param payload_size Size of the payload.
 *
 * @return GG_SUCCESS if the object could be created, or a negative error code.
 */
GG_Result GG_CoapResponder_Respond(GG_CoapResponder*          self,
                                   uint8_t                    code,
                                   GG_CoapMessageOptionParam* options,
                                   size_t                     options_count,
                                   const uint8_t*             payload,
                                   size_t                     payload_size);

/**
 * Release a responder.
 * This method must be called by asynchronous handlers that receive a responder object through
 * their OnRequest method when they no longer need to send a response (either because they have
 * already responded, or because they cannot respond).
 *
 * @param self The object on which this method is called.
 */
void GG_CoapResponder_Release(GG_CoapResponder* self);

/**
 * Split a URI path or query string into components.
 * The component delimiter is '/' for paths, and '&' for queries, but the function could be used
 * with other delimiters.
 * This function only performs basic splitting based on the delimiter (no percent-decoding or other string
 * processing). It isn't a general purpose URI parser, so it comes with limitations, including for instance
 * that it does not support having components that contain delimiters.
 * The components are populated as entries in an array of GG_CoapMessageOptionParam structs
 * supplied by the caller, with an option type set to GG_COAP_MESSAGE_OPTION_TYPE_UINT and
 * a string value. The string values point directly to the string supplied by the caller, so
 * the options returned by this function are only valid as long as the string that they point to
 * remains valid.
 * Leading delimiter characters will be ignored. A single trailing delimiter may be present and will
 * be ignored.
 *
 * Ex: splitting '/foo/bar' would result in a 'foo' and a 'bar' component option.
 *     splitting 'foo=1&bar=2' would result in a 'foo=1' and a 'bar=2' component option.
 *
 * @param path_or_query The string to split into components.
 * @param delimiter The delimiter for the components.
 * @param option_params The array in which components options will be returned, or NULL to just query how many
 * components are in the path.
 * @param option_params_count [in,out] On input, the size of the array that can receive the returned options.
 * On output, the number of components that were found.
 * @param option_number The CoAP option number to use for the options.
 * (ex: GG_COAP_MESSAGE_OPTION_URI_PATH or GG_COAP_MESSAGE_OPTION_URI_QUERY)
 *
 * @return GG_SUCCESS if the string was syntactically correct and there was enough space to return
 * all the components, GG_ERROR_INVALID_SYNTAX if the syntax was incorrect, GG_ERROR_NOT_ENOUGH_SPACE
 * if there were more components than entries in the receiving array, or another error code.
 */
GG_Result GG_Coap_SplitPathOrQuery(const char*                path_or_query,
                                   char                       delimiter,
                                   GG_CoapMessageOptionParam* option_params,
                                   size_t*                    option_params_count,
                                   uint32_t                   option_number);

/**
 * Helper function to clone an array of GG_CoapMessageOptionParam, making
 * copies of any data it points to. The copy is made in a single block
 * of memory that can be freed when no longer needed, by calling GG_FreeMemory
 *
 * @param options Array of options to clone.
 * @param option_count Number of options in the options array.
 *
 * @return Array of options, or NULL if there isn't enough memory.
 */
GG_CoapMessageOptionParam*
GG_Coap_CloneOptions(const GG_CoapMessageOptionParam* options, size_t option_count);

/*----------------------------------------------------------------------
|   Custom Extensions
+---------------------------------------------------------------------*/
/**
 * Extended error information, used with 4.xx and 5.xx responses.
 *
 * WARNING: the name_space and message fields may not be null-terminated, so it is important
 * to make use of the string size fields.
 *
 * NOTE: the `name_space` field is named like this so as not to conflicts with the reserved
 * `namespace` keyword in C++.
 *
 * This data is designed to be encoded to and decoded from a protobuf message with the following schema:
 *
 * message Error {
 *     optional string namespace = 1;
 *     optional sint32 code      = 2;
 *     optional string message   = 3;
 * }
 */
typedef struct {
    const char* name_space;      ///< Namespace for the error code (ex: "org.example.foo")
    size_t      name_space_size; ///< Size of the name_space string, or 0 if the string is null-terminated
    int32_t     code;            ///< Error code
    const char* message;         ///< Error message (may be NULL)
    size_t      message_size;    ///< Size of the message string, or 0 if the string is null-terminated
} GG_CoapExtendedError;

/**
 * Get the size of the protobuf-encoded representation of an extended error.
 *
 * @param self The object on which the method is invoked.
 */
size_t GG_CoapExtendedError_GetEncodedSize(const GG_CoapExtendedError* self);

/**
 * Encode an extended error as a protobuf message.
 * NOTE: the buffer passed to this method must be large enough to receive all the encoded
 * bytes. The number of bytes needed may be obtained by calling #GG_CoapExtendedError_GetEncodedSize.
 *
 * @param self The object on which the method is invoked.
 * @param buffer The buffer in which to write the encoded bytes.
 *
 * @return GG_SUCCESS if the call succeeded, or a negative error code.
 */
GG_Result GG_CoapExtendedError_Encode(const GG_CoapExtendedError* self, uint8_t* buffer);

/**
 * Decode an extended error from a protobuf message.
 *
 * NOTE: the strings pointed to by fields in the GG_CoapExtendedError point to
 * the buffer that is passed to this function, so the caller must not derefence
 * them after the payload buffer is no longer valid. If those strings are needed past
 * the lifetime of the payload buffer, the caller must make a copy before freeing or
 * otherwise invalidating the payload buffer.
 *
 * @param self The object on which the method is invoked.
 * @param payload The payload of the encoded protobuf message.
 * @param payload_size Size of the payload.
 *
 * @return GG_SUCCESS if the call succeeded, or a negative error code.
 * GG_ERROR_INVALID_FORMAT indicates that the payload isn't a decodable protbuf message.
 * GG_ERROR_INVALID_SYNTAX indicates that the payload is a valid protobuf message, but
 * doesn't comply with the expected schema.
 */
GG_Result GG_CoapExtendedError_Decode(GG_CoapExtendedError* self,
                                      const uint8_t*        payload,
                                      size_t                payload_size);

//! @}

#if defined(__cplusplus)
}
#endif

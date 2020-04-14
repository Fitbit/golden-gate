/**
 *
 * @file
 *
 * @copyright
 * Copyright 2017-2018 by Fitbit, Inc., all rights reserved.
 *
 *
 * @date 2018-05-18
 *
 * @details
 * CoAP client service implementation
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <string.h>
#include <stdio.h>

#include "fb_smo.h"
#include "xp/smo/gg_smo_allocator.h"
#include "xp/coap/gg_coap.h"
#include "xp/coap/gg_coap_message.h"
#include "xp/coap/gg_coap_blockwise.h"
#include "xp/loop/gg_loop.h"
#include "xp/common/gg_common.h"
#include "xp/remote/gg_remote.h"
#include "gg_coap_client_service.h"

/*----------------------------------------------------------------------
|   logging
+---------------------------------------------------------------------*/
GG_SET_LOCAL_LOGGER("gg.xp.service.coap.client")

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/
#define GG_COAP_CLIENT_SERVICE_MAX_PATH_COMPONENTS       8
#define GG_COAP_CLIENT_SERVICE_MAX_QUERY_COMPONENTS      8
#define GG_COAP_CLIENT_SERVICE_MAX_OPTIONS               8
#define GG_COAP_CLIENT_SERVICE_MAX_OPAQUE_OPTIONS_BUFFER 128
#define GG_COAP_CLIENT_SERVICE_MAX_AGENTS                8
#define GG_COAP_CLIENT_SERVICE_MAX_PATH_AND_QUERY_SIZE   64

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
/**
 * CoAP Client RPC error codes
 */
typedef enum {
    GG_COAP_CLIENT_RPC_ERROR_BUSY = 1
} GG_CoapClientServiceRcpErrorCode;

/**
 * CoAP Client State
 */
typedef enum {
    GG_COAP_CLIENT_STATE_IDLE = 0,
    GG_COAP_CLIENT_STATE_REQUEST_SENT,
    GG_COAP_CLIENT_STATE_RESPONSE_RECEIVED,
    GG_COAP_CLIENT_STATE_ERROR
} GG_CoapClientServiceState;

/**
 * CoAP Client Error Codes
 */
typedef enum {
    GG_COAP_CLIENT_NO_ERROR = 0,
    GG_COAP_CLIENT_UNSPECIFIED_ERROR,
    GG_COAP_CLIENT_TIMEOUT
} GG_CoapClientServiceErrorCode;

/**
 * CoAP Client Agent
 */
typedef struct {
    GG_IMPLEMENTS(GG_CoapBlockwiseResponseListener);
    GG_IMPLEMENTS(GG_CoapBlockSource);

    GG_Loop*                  loop;                     // loop to send messages to
    GG_CoapEndpoint*          endpoint;                 // coap endpoint
    GG_CoapClientServiceState state;                    // current state
    GG_Timestamp              request_sent_time;        // time at which request was sent out (ns)
    GG_DynamicBuffer*         request_payload;          // request payload to send
    size_t                    request_payload_size;     // size of the request payload
    GG_Timestamp              response_received_time;   // time at which response was received at handler (ns)
    GG_CoapMessage*           response;                 // last response received
    GG_DynamicBuffer*         response_payload;         // accumulated response payload
    size_t                    response_payload_size;    // accumulated response payload size
    uint32_t                  response_payload_crc;     // response CRC value
    bool                      discard_response_payload; // true if we just compute the CRC of the response
    GG_Result                 error_code;               // error code, if we get an error response

    GG_THREAD_GUARD_ENABLE_BINDING
}  GG_CoapClientAgent;

/**
 * CoAP Client Service
 */
struct GG_CoapClientService {
    GG_IMPLEMENTS(GG_RemoteSmoHandler);

    GG_CoapClientAgent agents[GG_COAP_CLIENT_SERVICE_MAX_AGENTS];
    GG_THREAD_GUARD_ENABLE_BINDING
};

/**
 * Used to pass parameters to a loop-invoked function for `SendRequest`
 */
typedef struct {
    GG_CoapClientAgent* agent;
    Fb_Smo*             rpc_params;
} GG_CoapClientAgent_SendRequestInvokeArgs;

/**
 * Used to pass parameters to a loop-invoked function for `GetStatus`
 */
typedef struct {
    GG_CoapClientAgent* agent;
    Fb_Smo**            rpc_result;
} GG_CoapClientAgent_GetStatusInvokeArgs;


/*----------------------------------------------------------------------
|   forward declarations
+---------------------------------------------------------------------*/
static void GG_CoapClientAgent_FreeResources(GG_CoapClientAgent* self);
static void GG_CoapClientAgent_Reset(GG_CoapClientAgent* self);

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

// ------------------------------------------------------------------------
static void
GG_CoapClientAgent_OnError(GG_CoapBlockwiseResponseListener* _self,
                           GG_Result                         error,
                           const char*                       message)
{
    GG_CoapClientAgent *self = GG_SELF(GG_CoapClientAgent, GG_CoapBlockwiseResponseListener);
    GG_COMPILER_UNUSED(message);

    // cleanup
    GG_CoapClientAgent_FreeResources(self);

    // update the state
    self->state = GG_COAP_CLIENT_STATE_ERROR;
    self->error_code = error;
}

// ------------------------------------------------------------------------
static void
GG_CoapClientAgent_OnResponseBlock(GG_CoapBlockwiseResponseListener* _self,
                                   GG_CoapMessageBlockInfo*          block_info,
                                   GG_CoapMessage*                   block_message)
{
    GG_CoapClientAgent *self = GG_SELF(GG_CoapClientAgent, GG_CoapBlockwiseResponseListener);

    // if the block has a payload, append it to the current response payload buffer
    size_t payload_size = GG_CoapMessage_GetPayloadSize(block_message);
    if (payload_size) {
        // update the CRC
        self->response_payload_crc = GG_Crc32(self->response_payload_crc,
                                              GG_CoapMessage_GetPayload(block_message),
                                              GG_CoapMessage_GetPayloadSize(block_message));

        // update the size
        self->response_payload_size += payload_size;

        // update the payload bytes
        if (!self->discard_response_payload) {
            // create a buffer if we don't already have one
            if (!self->response_payload) {
                GG_Result result = GG_DynamicBuffer_Create(payload_size, &self->response_payload);
                if (GG_FAILED(result)) {
                    GG_LOG_WARNING("failed to allocate buffer to response payload");
                    return;
                }
            }

            // now append to the buffer
            size_t current_size = GG_DynamicBuffer_GetDataSize(self->response_payload);
            GG_Result result = GG_DynamicBuffer_SetDataSize(self->response_payload, current_size + payload_size);
            if (GG_FAILED(result)) {
                GG_LOG_WARNING("failed to allocate buffer to response payload");
                return;
            }
            memcpy(GG_DynamicBuffer_UseData(self->response_payload) + current_size,
                   GG_CoapMessage_GetPayload(block_message),
                   GG_CoapMessage_GetPayloadSize(block_message));
        }
    }

    // check if this is the last block or just progress
    if (!block_info->more) {
        // last block, we're done
        self->state                  = GG_COAP_CLIENT_STATE_RESPONSE_RECEIVED;
        self->response_received_time = GG_System_GetCurrentTimestamp();

        // if this is the last block, keep a copy of the response, replacing the previous one, if any.
        if (self->response) {
            GG_CoapMessage_Destroy(self->response);
            self->response = NULL;
        }
        GG_Buffer* datagram = NULL;
        if (GG_SUCCEEDED(GG_CoapMessage_ToDatagram(block_message, &datagram))) {
            GG_CoapMessage_CreateFromDatagram(datagram, &self->response);
            GG_Buffer_Release(datagram);
        }
    }
}

// ------------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(GG_CoapClientAgent, GG_CoapBlockwiseResponseListener) {
    .OnResponseBlock = GG_CoapClientAgent_OnResponseBlock,
    .OnError         = GG_CoapClientAgent_OnError
};

// ------------------------------------------------------------------------
static GG_Result
GG_CoapClientAgent_GetRequestDataSize(GG_CoapBlockSource* _self,
                                      size_t              offset,
                                      size_t*             data_size,
                                      bool*               more)
{
    GG_CoapClientAgent *self = GG_SELF(GG_CoapClientAgent, GG_CoapBlockSource);

    return GG_CoapMessageBlockInfo_AdjustAndGetChunkSize(offset,
                                                         data_size,
                                                         more,
                                                         self->request_payload_size);
}

// ------------------------------------------------------------------------
static GG_Result
GG_CoapClientAgent_GetRequestData(GG_CoapBlockSource* _self,
                                  size_t              offset,
                                  size_t              data_size,
                                  void*               data)
{
    GG_CoapClientAgent *self = GG_SELF(GG_CoapClientAgent, GG_CoapBlockSource);

    // check bounds
    if (!self->request_payload || offset + data_size > self->request_payload_size) {
        return GG_ERROR_INTERNAL;
    }

    // produce the data
    if (self->request_payload) {
        // copy the data
        memcpy(data, GG_DynamicBuffer_GetData(self->request_payload) + offset, data_size);
    } else {
        // generate a data pattern
        for (size_t i = 0; i < data_size; i++) {
            ((uint8_t*)data)[i] = (uint8_t)(offset + i);
        }
    }

    return GG_SUCCESS;
}

// ------------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(GG_CoapClientAgent, GG_CoapBlockSource) {
    .GetDataSize = GG_CoapClientAgent_GetRequestDataSize,
    .GetData     = GG_CoapClientAgent_GetRequestData
};

// ------------------------------------------------------------------------
static void
GG_CoapClientAgent_FreeResources(GG_CoapClientAgent* self)
{
    if (self->request_payload) {
        GG_DynamicBuffer_Release(self->request_payload);
        self->request_payload = NULL;
    }
    GG_CoapMessage_Destroy(self->response);
    self->response = NULL;
    if (self->response_payload) {
        GG_DynamicBuffer_Release(self->response_payload);
        self->response_payload = NULL;
    }
}

// ------------------------------------------------------------------------
static void
GG_CoapClientAgent_Reset(GG_CoapClientAgent* self)
{
    GG_CoapClientAgent_FreeResources(self);
    self->state = GG_COAP_CLIENT_STATE_IDLE;
    self->error_code = GG_SUCCESS;
    self->request_sent_time = 0;
    self->request_payload_size = 0;
    self->response_payload_size = 0;
    self->response_payload_crc = 0;
    self->response_received_time = 0;
}

// ------------------------------------------------------------------------
static int
GG_CoapClientAgent_SendRequest_(void* _args)
{
    GG_CoapClientAgent_SendRequestInvokeArgs* args       = _args;
    GG_CoapClientAgent*                       self       = args->agent;
    Fb_Smo*                                   rpc_params = args->rpc_params;
    GG_Result                                 result;

    // check if there's a pending request
    if (self->state == GG_COAP_CLIENT_STATE_REQUEST_SENT) {
        // error, we can only have one pending request at a time
        return GG_ERROR_WOULD_BLOCK;
    }

    // reset before making a new request
    GG_CoapClientAgent_Reset(self);

    // check that we have the required params
    Fb_Smo* method_param = Fb_Smo_GetChildByName(rpc_params, "method");
    Fb_Smo* path_param   = Fb_Smo_GetChildByName(rpc_params, "path");
    if (!method_param || Fb_Smo_GetType(method_param) != FB_SMO_TYPE_STRING ||
        !path_param   || Fb_Smo_GetType(path_param)   != FB_SMO_TYPE_STRING) {
        return GG_ERROR_INVALID_PARAMETERS;
    }

    // parse the method name
    uint8_t method;
    if (!strcmp(Fb_Smo_GetValueAsString(method_param), "GET")) {
        method = GG_COAP_METHOD_GET;
    } else if (!strcmp(Fb_Smo_GetValueAsString(method_param), "PUT")) {
        method = GG_COAP_METHOD_PUT;
    } else if (!strcmp(Fb_Smo_GetValueAsString(method_param), "POST")) {
        method = GG_COAP_METHOD_POST;
    } else if (!strcmp(Fb_Smo_GetValueAsString(method_param), "DELETE")) {
        method = GG_COAP_METHOD_DELETE;
    } else {
        GG_LOG_WARNING("invalid method");
        return GG_ERROR_INVALID_PARAMETERS;
    }

    // delimit the path and query
    if (strlen(Fb_Smo_GetValueAsString(path_param)) > GG_COAP_CLIENT_SERVICE_MAX_PATH_AND_QUERY_SIZE) {
        return GG_ERROR_INVALID_PARAMETERS;
    }
    char path_and_query[GG_COAP_CLIENT_SERVICE_MAX_PATH_AND_QUERY_SIZE + 1];
    strncpy(path_and_query, Fb_Smo_GetValueAsString(path_param), sizeof(path_and_query));
    path_and_query[sizeof(path_and_query) - 1] = '\0';
    const char* path = path_and_query;
    char* query = strchr(path_and_query, '?');
    if (query) {
        *query++ = '\0'; // put a null terminator at the end of the path
    }

    // allocate stack space for the path, query and options
    size_t options_count = 0;
    GG_CoapMessageOptionParam options[GG_COAP_CLIENT_SERVICE_MAX_PATH_COMPONENTS +
                                      GG_COAP_CLIENT_SERVICE_MAX_QUERY_COMPONENTS +
                                      GG_COAP_CLIENT_SERVICE_MAX_OPTIONS];

    // parse the path
    size_t path_options_count = GG_COAP_CLIENT_SERVICE_MAX_PATH_COMPONENTS;
    result = GG_Coap_SplitPathOrQuery(path,
                                      '/',
                                      options,
                                      &path_options_count,
                                      GG_COAP_MESSAGE_OPTION_URI_PATH);
    if (GG_FAILED(result)) {
        GG_LOG_WARNING("GG_Coap_SplitPath returned %d", result);
        return result;
    }
    if (!path_options_count) {
        return GG_ERROR_INVALID_PARAMETERS;
    }
    options_count += path_options_count;

    // parse the query
    if (query) {
        size_t query_options_count = GG_COAP_CLIENT_SERVICE_MAX_QUERY_COMPONENTS;
        result = GG_Coap_SplitPathOrQuery(query,
                                          '&',
                                          &options[options_count],
                                          &query_options_count,
                                          GG_COAP_MESSAGE_OPTION_URI_QUERY);
        if (GG_FAILED(result)) {
            GG_LOG_WARNING("GG_Coap_SplitPath returned %d", result);
            return result;
        }
        if (!query_options_count) {
            return GG_ERROR_INVALID_PARAMETERS;
        }
        options_count += query_options_count;
    }

    // parse the options
    Fb_Smo* options_param = Fb_Smo_GetChildByName(rpc_params, "options");
    uint8_t opaque_options_data[GG_COAP_CLIENT_SERVICE_MAX_OPAQUE_OPTIONS_BUFFER];
    size_t  opaque_options_data_size = 0;
    if (options_param && Fb_Smo_GetType(options_param) == FB_SMO_TYPE_ARRAY) {
        for (Fb_Smo* option = Fb_Smo_GetFirstChild(options_param);
             option && options_count < GG_COAP_CLIENT_SERVICE_MAX_PATH_COMPONENTS + GG_COAP_CLIENT_SERVICE_MAX_OPTIONS;
             option = Fb_Smo_GetNext(option)) {
            Fb_Smo* option_number = Fb_Smo_GetChildByName(option, "number");
            if (!option_number) {
                GG_LOG_WARNING("option has no number");
                return GG_ERROR_INVALID_PARAMETERS;
            }
            options[options_count].option.number = (uint32_t)Fb_Smo_GetValueAsInteger(option_number);
            Fb_Smo* option_value = Fb_Smo_GetChildByName(option, "value");
            if (option_value) {
                // figure out the type
                // NOTE: we don't check for byte array types here, because the source is JSON, which doesn't
                // have a native byte array type, so byte arrays are actually hex-encoded into a string
                if (Fb_Smo_GetType(option_value) == FB_SMO_TYPE_INTEGER) {
                    options[options_count].option.type = GG_COAP_MESSAGE_OPTION_TYPE_UINT;
                    options[options_count].option.value.uint = (uint32_t)Fb_Smo_GetValueAsInteger(option_value);
                } else if (Fb_Smo_GetType(option_value) == FB_SMO_TYPE_STRING) {
                    // check if this is a string or a hex-encoded byte array
                    Fb_Smo* value_is_opaque = Fb_Smo_GetChildByName(option, "value_is_opaque");
                    if (value_is_opaque && Fb_Smo_GetValueAsSymbol(value_is_opaque) == FB_SMO_SYMBOL_TRUE) {
                        options[options_count].option.type = GG_COAP_MESSAGE_OPTION_TYPE_OPAQUE;

                        // decode the hex-encoded opaque option
                        const char* value_hex = Fb_Smo_GetValueAsString(option_value);
                        size_t value_size = strlen(value_hex) / 2;
                        options[options_count].option.value.opaque.size = value_size;
                        if (value_size) {
                            if (opaque_options_data_size + value_size <= sizeof(opaque_options_data)) {
                                // there's enough space to store this
                                uint8_t* buffer = &opaque_options_data[opaque_options_data_size];
                                options[options_count].option.value.opaque.bytes = buffer;
                                GG_HexToBytes(value_hex, 0, buffer);
                                opaque_options_data_size += value_size;
                            } else {
                                // not enough space
                                GG_LOG_WARNING("skipping opaque option, not enough buffer space");
                                return GG_ERROR_OUT_OF_MEMORY;
                            }
                        }
                    } else {
                        options[options_count].option.type = GG_COAP_MESSAGE_OPTION_TYPE_STRING;
                        options[options_count].option.value.string.chars = Fb_Smo_GetValueAsString(option_value);
                        options[options_count].option.value.string.length = 0; // null-terminated
                    }
                } else {
                    GG_LOG_WARNING("invalid option type");
                    return GG_ERROR_INVALID_PARAMETERS;
                }
            } else {
                // empty option
                options[options_count].option.type = GG_COAP_MESSAGE_OPTION_TYPE_EMPTY;
            }

            ++options_count;
        }
    }

    // parse the payload
    Fb_Smo* payload_param = Fb_Smo_GetChildByName(rpc_params, "payload");
    if (payload_param) {
        // check that the method allows payloads
        if (method != GG_COAP_METHOD_POST && method != GG_COAP_METHOD_PUT) {
            GG_LOG_WARNING("payload not allowed with this method");
            return GG_ERROR_INVALID_PARAMETERS;
        }

        // depending on the type, the payload argument may be a payload size, or the payload bytes in hex
        Fb_SmoType param_type = Fb_Smo_GetType(payload_param);
        if (param_type == FB_SMO_TYPE_STRING) {
            // decode the hex-encoded payload
            const char* payload_hex = Fb_Smo_GetValueAsString(payload_param);
            size_t payload_hex_length = strlen(payload_hex);
            size_t payload_size = payload_hex_length / 2;
            if (payload_size) {
                result = GG_DynamicBuffer_Create(payload_size, &self->request_payload);
                if (GG_FAILED(result)) {
                    GG_LOG_WARNING("cannot create buffer for the request payload");
                } else {
                    self->request_payload_size = payload_size;
                    GG_DynamicBuffer_SetDataSize(self->request_payload, payload_size);
                    result = GG_HexToBytes(payload_hex, 0, GG_DynamicBuffer_UseData(self->request_payload));
                    if (GG_FAILED(result)) {
                        GG_LOG_WARNING("invalid hex payload");
                        return GG_ERROR_INVALID_PARAMETERS;
                    }
                }
            } else {
                return GG_ERROR_INVALID_PARAMETERS;
            }
        } else if (param_type == FB_SMO_TYPE_INTEGER) {
            self->request_payload_size = (size_t)Fb_Smo_GetValueAsInteger(payload_param);
        } else {
            return GG_ERROR_INVALID_PARAMETERS;
        }
    }

    // check if we need to discard the response payload
    Fb_Smo* discard_response_payload = Fb_Smo_GetChildByName(rpc_params, "discard_response_payload");
    if (discard_response_payload && Fb_Smo_GetValueAsSymbol(discard_response_payload) == FB_SMO_SYMBOL_TRUE) {
        self->discard_response_payload = true;
    }

    // send the request
    GG_CoapRequestHandle handle = 0;
    self->request_sent_time = GG_System_GetCurrentTimestamp();
    result = GG_CoapEndpoint_SendBlockwiseRequest(self->endpoint,
                                                  method,
                                                  options,
                                                  (unsigned int)options_count,
                                                  payload_param ? GG_CAST(self, GG_CoapBlockSource) : NULL,
                                                  0,
                                                  NULL,
                                                  GG_CAST(self, GG_CoapBlockwiseResponseListener),
                                                  &handle);
    if (GG_FAILED(result)) {
        GG_LOG_WARNING("GG_CoapEndpoint_SendRequest failed (%d)", result);
        self->state = GG_COAP_CLIENT_STATE_ERROR;
        self->error_code = result;
        GG_CoapClientAgent_FreeResources(self);
        return result;
    }

    // we're now waiting for a response
    self->state = GG_COAP_CLIENT_STATE_REQUEST_SENT;

    return GG_SUCCESS;
}

// ------------------------------------------------------------------------
static GG_Result
GG_CoapClientAgent_SendRequest(GG_CoapClientAgent* self, Fb_Smo* rpc_params, GG_JsonRpcErrorCode* rpc_error_code)
{
    GG_ASSERT(self);
    GG_ASSERT(self->loop);
    GG_ASSERT(self->endpoint);

    int invoke_result = 0;
    GG_CoapClientAgent_SendRequestInvokeArgs invoke_args = {
        .agent      = self,
        .rpc_params = rpc_params
    };
    GG_Result result = GG_Loop_InvokeSync(self->loop,
                                          GG_CoapClientAgent_SendRequest_,
                                          &invoke_args,
                                          &invoke_result);
    GG_CHECK_SEVERE(result);

    if (invoke_result == GG_ERROR_WOULD_BLOCK) {
        *rpc_error_code = (GG_JsonRpcErrorCode)GG_COAP_CLIENT_RPC_ERROR_BUSY;
        return GG_FAILURE;
    } else {
        return invoke_result;
    }
}

// ------------------------------------------------------------------------
// Convert a byte array into a hex-encoded SMO String
// ------------------------------------------------------------------------
static Fb_Smo*
GG_CoapClientService_EncodeBytes(const uint8_t* data, size_t data_size)
{
    GG_DynamicBuffer* hex = NULL;
    GG_Result result = GG_DynamicBuffer_Create(2 * data_size, &hex);
    if (GG_FAILED(result)) {
        return NULL;
    }

    GG_DynamicBuffer_SetDataSize(hex, data_size * 2);
    GG_BytesToHex(data, data_size, (char*)GG_DynamicBuffer_UseData(hex), false);
    Fb_Smo* encoded = Fb_Smo_CreateString(&GG_SmoHeapAllocator,
                                          (const char*)GG_DynamicBuffer_GetData(hex),
                                          (unsigned int)(2 * data_size));
    GG_DynamicBuffer_Release(hex);
    return encoded;
}

// ------------------------------------------------------------------------
static int
GG_CoapClientAgent_GetStatus_(void* _args)
{
    GG_CoapClientAgent_GetStatusInvokeArgs* args       = _args;
    GG_CoapClientAgent*                     self       = args->agent;
    Fb_Smo**                                rpc_result = args->rpc_result;
    GG_Result                               result     = GG_SUCCESS;

    // state field
    Fb_Smo* state = Fb_Smo_CreateInteger(&GG_SmoHeapAllocator, self->state);

    // response field
    Fb_Smo* response = NULL;
    Fb_Smo* time_elapsed = NULL;
    if (self->state == GG_COAP_CLIENT_STATE_RESPONSE_RECEIVED) {
        GG_ASSERT(self->response);

        // time_elapsed field
        time_elapsed = Fb_Smo_CreateFloat(&GG_SmoHeapAllocator,
                                          ((double)self->response_received_time -
                                           (double)self->request_sent_time) /
                                          (double)GG_NANOSECONDS_PER_SECOND);
        if (time_elapsed == NULL) {
            result = GG_ERROR_OUT_OF_MEMORY;
        }

        // map from a "packed" code form to an "integer" code form
        uint8_t packed_code = GG_CoapMessage_GetCode(self->response);
        Fb_Smo* code = Fb_Smo_CreateInteger(&GG_SmoHeapAllocator, 100 * ((packed_code >> 5) & 7) + (packed_code & 31));
        if (code == NULL) {
            result = GG_ERROR_OUT_OF_MEMORY;
        }

        // response payload
        Fb_Smo* payload = NULL;
        if (self->response_payload) {
            size_t payload_size = GG_DynamicBuffer_GetDataSize(self->response_payload);
            if (payload_size) {
                // hex-encode the payload
                payload = GG_CoapClientService_EncodeBytes(GG_DynamicBuffer_GetData(self->response_payload),
                                                           payload_size);
                if (payload == NULL) {
                    result = GG_ERROR_OUT_OF_MEMORY;
                }
            }
        }

        // response payload size (may be redundant if we have a payload field, but that's Ok)
        Fb_Smo* payload_size = Fb_Smo_CreateInteger(&GG_SmoHeapAllocator, (int64_t)self->response_payload_size);

        // response payload CRC
        Fb_Smo* payload_crc = Fb_Smo_CreateInteger(&GG_SmoHeapAllocator, (int64_t)self->response_payload_crc);

        // response options
        Fb_Smo* options = NULL;
        GG_CoapMessageOptionIterator option_iterator;
        GG_CoapMessage_InitOptionIterator(self->response, GG_COAP_MESSAGE_OPTION_ITERATOR_FILTER_ANY, &option_iterator);
        while (option_iterator.option.number) {
            Fb_Smo* option = Fb_Smo_Create(&GG_SmoHeapAllocator, "{number=i}", (int)option_iterator.option.number);
            if (!option) {
                continue;
            }

            // convert the option value to an SMO object
            Fb_Smo* option_value = NULL;
            switch (option_iterator.option.type) {
                case GG_COAP_MESSAGE_OPTION_TYPE_UINT:
                    option_value = Fb_Smo_CreateInteger(&GG_SmoHeapAllocator, option_iterator.option.value.uint);
                    break;

                case GG_COAP_MESSAGE_OPTION_TYPE_STRING:
                    option_value = Fb_Smo_CreateString(&GG_SmoHeapAllocator,
                                                       option_iterator.option.value.string.chars,
                                                       (unsigned int)option_iterator.option.value.string.length);
                    break;

                case GG_COAP_MESSAGE_OPTION_TYPE_OPAQUE: {
                    option_value = GG_CoapClientService_EncodeBytes(option_iterator.option.value.opaque.bytes,
                                                                    option_iterator.option.value.opaque.size);
                    Fb_Smo* value_is_opaque = Fb_Smo_CreateSymbol(&GG_SmoHeapAllocator, FB_SMO_SYMBOL_TRUE);
                    if (value_is_opaque) {
                        Fb_Smo_AddChild(option, "value_is_opaque", 0, value_is_opaque);
                    }
                    break;
                }

                case GG_COAP_MESSAGE_OPTION_TYPE_EMPTY:
                    break;
            }

            // set the option value
            if (option_value) {
                Fb_Smo_AddChild(option, "value", 0, option_value);
            }

            // add the option to the list
            if (options == NULL) {
                options = Fb_Smo_CreateArray(&GG_SmoHeapAllocator);
            }
            if (!options || Fb_Smo_AddChild(options, NULL, 0, option) != FB_SMO_SUCCESS) {
                Fb_Smo_Destroy(option);
            }

            // next
            GG_CoapMessage_StepOptionIterator(self->response, &option_iterator);
        }

        // populate the response object
        response = Fb_Smo_CreateObject(&GG_SmoHeapAllocator);
        if (response) {
            if (code)         Fb_Smo_AddChild(response, "code", 0, code);
            if (payload)      Fb_Smo_AddChild(response, "payload", 0, payload);
            if (payload_size) Fb_Smo_AddChild(response, "payload_size", 0, payload_size);
            if (payload_crc)  Fb_Smo_AddChild(response, "payload_crc", 0, payload_crc);
            if (options)      Fb_Smo_AddChild(response, "options", 0, options);
        } else {
            if (code)         Fb_Smo_Destroy(code);
            if (payload)      Fb_Smo_Destroy(payload);
            if (payload_size) Fb_Smo_Destroy(payload_size);
            if (payload_crc)  Fb_Smo_Destroy(payload_crc);
            if (options)      Fb_Smo_Destroy(options);
            result = GG_ERROR_OUT_OF_MEMORY;
        }
    }

    // error_code field
    Fb_Smo* error_code = NULL;
    if (self->state == GG_COAP_CLIENT_STATE_ERROR) {
        int status_error_code = GG_COAP_CLIENT_NO_ERROR;
        switch (self->error_code) {
            case GG_ERROR_TIMEOUT:
                status_error_code = GG_COAP_CLIENT_TIMEOUT;
                break;

            default:
                status_error_code = GG_COAP_CLIENT_UNSPECIFIED_ERROR;
                break;
        }
        error_code = Fb_Smo_CreateInteger(&GG_SmoHeapAllocator, status_error_code);
        if (error_code == NULL) {
            result = GG_ERROR_OUT_OF_MEMORY;
        }
    }

    // build the final result object
    if (GG_SUCCEEDED(result)) {
        *rpc_result = Fb_Smo_CreateObject(&GG_SmoHeapAllocator);
        if (*rpc_result == NULL) {
            result = GG_ERROR_OUT_OF_MEMORY;
        }
    }
    if (*rpc_result) {
        if (state)        Fb_Smo_AddChild(*rpc_result, "state", 0, state);
        if (time_elapsed) Fb_Smo_AddChild(*rpc_result, "time_elapsed", 0, time_elapsed);
        if (response)     Fb_Smo_AddChild(*rpc_result, "response", 0, response);
        if (error_code)   Fb_Smo_AddChild(*rpc_result, "error_code", 0, error_code);

        return GG_SUCCESS;
    }

    // error condition, cleanup
    if (state)        Fb_Smo_Destroy(state);
    if (time_elapsed) Fb_Smo_Destroy(time_elapsed);
    if (response)     Fb_Smo_Destroy(response);
    if (error_code)   Fb_Smo_Destroy(error_code);

    return result;
}

// ------------------------------------------------------------------------
static GG_Result
GG_CoapClientAgent_GetStatus(GG_CoapClientAgent* self, Fb_Smo** rpc_result)
{
    GG_ASSERT(self);
    GG_ASSERT(self->loop);
    GG_ASSERT(self->endpoint);

    int invoke_result = 0;
    GG_CoapClientAgent_GetStatusInvokeArgs invoke_args = {
        .agent      = self,
        .rpc_result = rpc_result
    };
    GG_Result result = GG_Loop_InvokeSync(self->loop,
                                          GG_CoapClientAgent_GetStatus_,
                                          &invoke_args, &invoke_result);
    GG_CHECK_SEVERE(result);

    return invoke_result;
}

// ------------------------------------------------------------------------
//  Shared Remote API handler
// ------------------------------------------------------------------------
static GG_Result
GG_CoapClientService_HandleRequest(GG_RemoteSmoHandler* _self,
                                   const char*          request_method,
                                   Fb_Smo*              request_params,
                                   GG_JsonRpcErrorCode* rpc_error_code,
                                   Fb_Smo**             rpc_result)
{
    GG_COMPILER_UNUSED(rpc_error_code);

    GG_CoapClientService* self = GG_SELF(GG_CoapClientService, GG_RemoteSmoHandler);

    // pick the agent to use (use agent 0 by default)
    GG_CoapClientAgent* agent = &self->agents[0];
    if (request_params) {
        Fb_Smo* client = Fb_Smo_GetChildByName(request_params, "client");
        if (client && Fb_Smo_GetType(client) == FB_SMO_TYPE_INTEGER) {
            unsigned int agent_index = (unsigned int)Fb_Smo_GetValueAsInteger(client);
            if (agent_index >= GG_COAP_CLIENT_SERVICE_MAX_AGENTS) {
                return GG_ERROR_INVALID_PARAMETERS;
            }
            agent = &self->agents[agent_index];
        }
    }

    if (!strcmp(request_method, GG_COAP_CLIENT_SERVICE_SEND_REQUEST_METHOD)) {
        return GG_CoapClientAgent_SendRequest(agent, request_params, rpc_error_code);
    } else if (!strcmp(request_method, GG_COAP_CLIENT_SERVICE_GET_STATUS_METHOD)) {
        return GG_CoapClientAgent_GetStatus(agent, rpc_result);
    } else {
        // we should never have been called for this method
        return GG_ERROR_INTERNAL;
    }
}

// ------------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(GG_CoapClientService, GG_RemoteSmoHandler) {
    GG_CoapClientService_HandleRequest
};

// ------------------------------------------------------------------------
GG_Result
GG_CoapClientService_Register(GG_CoapClientService* self, GG_RemoteShell* shell)
{
    GG_Result result;

    // register RPC methods
    result = GG_RemoteShell_RegisterSmoHandler(shell,
                                               GG_COAP_CLIENT_SERVICE_SEND_REQUEST_METHOD,
                                               GG_CAST(self, GG_RemoteSmoHandler));
    GG_CHECK_SEVERE(result);

    result = GG_RemoteShell_RegisterSmoHandler(shell,
                                               GG_COAP_CLIENT_SERVICE_GET_STATUS_METHOD,
                                               GG_CAST(self, GG_RemoteSmoHandler));
    GG_CHECK_SEVERE(result);

    return GG_SUCCESS;
}

GG_Result
GG_CoapClientService_Unregister(GG_CoapClientService* self, GG_RemoteShell* shell)
{
    GG_Result result;

    // unregister RPC methods
    result = GG_RemoteShell_UnregisterSmoHandler(shell,
                                                 GG_COAP_CLIENT_SERVICE_SEND_REQUEST_METHOD,
                                                 GG_CAST(self, GG_RemoteSmoHandler));

    if (GG_FAILED(result)) {
        return result;
    }

    return GG_RemoteShell_UnregisterSmoHandler(shell,
                                               GG_COAP_CLIENT_SERVICE_GET_STATUS_METHOD,
                                               GG_CAST(self, GG_RemoteSmoHandler));
}


// ------------------------------------------------------------------------
GG_RemoteSmoHandler*
GG_CoapClientService_AsRemoteSmoHandler(GG_CoapClientService* self)
{
    return GG_CAST(self, GG_RemoteSmoHandler);
}

// ------------------------------------------------------------------------
// Create a GG_CoapClientService object (called in any
// thread context, typically the GG_RemoteShell thread context)
// ------------------------------------------------------------------------
GG_Result
GG_CoapClientService_Create(GG_Loop*               loop,
                            GG_CoapEndpoint*       endpoint,
                            GG_CoapClientService** service)
{
    GG_CoapClientService* self = GG_AllocateZeroMemory(sizeof(GG_CoapClientService));
    if (self == NULL) {
        GG_LOG_SEVERE("Failed to allocate a new coap client service object");
        return GG_FAILURE;
    }

    // init all the agents
    for (unsigned int i = 0; i < GG_COAP_CLIENT_SERVICE_MAX_AGENTS; i++) {
        self->agents[i].loop     = loop;
        self->agents[i].endpoint = endpoint;
        GG_SET_INTERFACE(&self->agents[i], GG_CoapClientAgent, GG_CoapBlockwiseResponseListener);
        GG_SET_INTERFACE(&self->agents[i], GG_CoapClientAgent, GG_CoapBlockSource);
    }

    GG_SET_INTERFACE(self, GG_CoapClientService, GG_RemoteSmoHandler);

    // Bind the object to the thread that created it
    GG_THREAD_GUARD_BIND(self);

    *service = self;

    return GG_SUCCESS;
}

// ------------------------------------------------------------------------
// Destroy a GG_CoapClientService object (called in any
// thread context, typically the GG_RemoteShell thread context)
// ------------------------------------------------------------------------
void
GG_CoapClientService_Destroy(GG_CoapClientService* self)
{
    if (self == NULL) return;

    GG_THREAD_GUARD_CHECK_BINDING(self);

    for (unsigned int i = 0; i < GG_COAP_CLIENT_SERVICE_MAX_AGENTS; i++) {
        GG_CoapClientAgent_FreeResources(&self->agents[i]);
    }

    GG_ClearAndFreeObject(self, 1);
}

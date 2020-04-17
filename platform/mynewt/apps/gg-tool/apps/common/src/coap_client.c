/**
 * @file
 *
 * @copyright
 * Copyright 2017-2020 Fitbit, Inc
 * SPDX-License-Identifier: Apache-2.0
 *
 * @date 2018-08-01
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "console/console.h"

#include "xp/coap/gg_coap.h"
#include "xp/coap/gg_coap_blockwise.h"
#include "xp/common/gg_buffer.h"
#include "xp/common/gg_memory.h"
#include "xp/common/gg_port.h"
#include "xp/common/gg_results.h"
#include "xp/common/gg_utils.h"
#include "xp/loop/gg_loop.h"

#include "coap_client.h"

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/
#define GG_COAP_CLIENT_MAX_PATH_COMPONENTS  16
#define GG_COAP_CLIENT_MAX_QUERY_COMPONENTS 16
#define GG_COAP_CLIENT_MAX_OPTIONS_COUNT    16
#define GG_COAP_CLIENT_DUMP_CHUNK_SIZE      16
#define GG_COAP_CLIENT_MTU                  1280
#define GG_COAP_MAX_SIMPLE_REQUEST_PAYLOAD  1024

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
typedef struct {
    GG_IMPLEMENTS(GG_CoapResponseListener);
    GG_IMPLEMENTS(GG_CoapBlockwiseResponseListener);

    GG_Loop*         loop;
    GG_CoapEndpoint* endpoint;

    size_t           expected_offset;
} CoapClient;

typedef struct {
    GG_IMPLEMENTS(GG_CoapBlockSource);

    GG_DynamicBuffer *data;
} MemSource;

typedef struct {
    bool                       use_blockwise;
    GG_CoapBlockSource*        payload_source;
    GG_CoapMethod              method;
    char*                      uri;
    GG_CoapMessageOptionParam* request_options;
    size_t                     request_options_count;
    size_t                     preferred_block_size;
} CoapRequest;

/*----------------------------------------------------------------------
|   globals
+---------------------------------------------------------------------*/
static CoapClient client;
static MemSource mem_source;
static CoapRequest request_info;
static bool verbose;
static bool hex_dump;

/*----------------------------------------------------------------------
|   forward declarations
+---------------------------------------------------------------------*/

static void CoapClient_Cleanup(void);

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

//----------------------------------------------------------------------
static GG_Result
MemSource_GetDataSize(GG_CoapBlockSource* _self,
                      size_t              offset,
                      size_t*             data_size,
                      bool*               more)
{
    MemSource* self = GG_SELF(MemSource, GG_CoapBlockSource);

    size_t buf_size = GG_DynamicBuffer_GetDataSize(self->data);

    return GG_CoapMessageBlockInfo_AdjustAndGetChunkSize(offset, data_size, more, buf_size);
}

//----------------------------------------------------------------------
static GG_Result
MemSource_GetData(GG_CoapBlockSource* _self,
                  size_t              offset,
                  size_t              data_size,
                  void*               data)
{
    MemSource* self = GG_SELF(MemSource, GG_CoapBlockSource);

    size_t buf_size = GG_DynamicBuffer_GetDataSize(self->data);
    const uint8_t* buf_data = GG_DynamicBuffer_GetData(self->data);

    if (offset + data_size <= buf_size) {
        memcpy(data, buf_data + offset, data_size);
    } else {
        return GG_ERROR_OUT_OF_RANGE;
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(MemSource, GG_CoapBlockSource) {
    .GetDataSize = MemSource_GetDataSize,
    .GetData     = MemSource_GetData
};

//----------------------------------------------------------------------
static void
MemSource_Deinit(MemSource* self)
{
    if (self->data != NULL) {
        GG_DynamicBuffer_Release(self->data);
        self->data = NULL;
    }
}

//----------------------------------------------------------------------
static GG_Result
MemSource_ResetData(MemSource* self, size_t buffer_size)
{
    MemSource_Deinit(self);

    return GG_DynamicBuffer_Create(buffer_size, &self->data);
}

//----------------------------------------------------------------------
static GG_Result
MemSource_SetData(MemSource* self, const uint8_t* data, size_t data_size)
{
    GG_Result rc;

    if (data == NULL || data_size == 0) {
        return GG_ERROR_INVALID_PARAMETERS;
    }

    if (self->data == NULL) {
        rc = GG_DynamicBuffer_Create(data_size, &self->data);
        if (GG_FAILED(rc)) {
            return rc;
        }
    }

    return GG_DynamicBuffer_SetData(self->data, data, data_size);
}

//----------------------------------------------------------------------
static GG_Result
MemSource_AppendHexString(MemSource* self, char *data, size_t data_size)
{
    uint8_t* buf;
    GG_Result rc;

    if (data == NULL || data_size == 0) {
        return GG_ERROR_INVALID_PARAMETERS;
    }

    buf = GG_AllocateMemory(data_size / 2);
    if (buf == NULL) {
        return GG_ERROR_OUT_OF_MEMORY;
    }

    rc = GG_HexToBytes(data, data_size, buf);
    if (GG_FAILED(rc)) {
        goto append_exit;
    }

    if (self->data == NULL) {
        rc = GG_DynamicBuffer_Create(data_size / 2, &self->data);
        if (GG_FAILED(rc)) {
            goto append_exit;
        }
    }

    rc = GG_DynamicBuffer_AppendData(self->data, buf, data_size / 2);

append_exit:
    GG_FreeMemory(buf);

    return rc;
}

//----------------------------------------------------------------------
static size_t
MemSource_GetPayloadSize(MemSource* self)
{
    return self->data != NULL ? GG_DynamicBuffer_GetDataSize(self->data) : 0;
}

//----------------------------------------------------------------------
static const char*
GetOptionName(uint32_t option_number)
{
    switch (option_number) {
        case GG_COAP_MESSAGE_OPTION_IF_MATCH:       return "If-Match";
        case GG_COAP_MESSAGE_OPTION_URI_HOST:       return "Uri-Host";
        case GG_COAP_MESSAGE_OPTION_ETAG:           return "ETag";
        case GG_COAP_MESSAGE_OPTION_IF_NONE_MATCH:  return "If-None-Match";
        case GG_COAP_MESSAGE_OPTION_URI_PORT:       return "Uri-Port";
        case GG_COAP_MESSAGE_OPTION_LOCATION_PATH:  return "Location-Path";
        case GG_COAP_MESSAGE_OPTION_URI_PATH:       return "Uri-Path";
        case GG_COAP_MESSAGE_OPTION_CONTENT_FORMAT: return "Content-Format";
        case GG_COAP_MESSAGE_OPTION_MAX_AGE:        return "Max-Age";
        case GG_COAP_MESSAGE_OPTION_URI_QUERY:      return "Uri-Query";
        case GG_COAP_MESSAGE_OPTION_ACCEPT:         return "Accept";
        case GG_COAP_MESSAGE_OPTION_LOCATION_QUERY: return "Location-Query";
        case GG_COAP_MESSAGE_OPTION_PROXY_URI:      return "Proxy-Uri";
        case GG_COAP_MESSAGE_OPTION_PROXY_SCHEME:   return "Proxy-Scheme";
        case GG_COAP_MESSAGE_OPTION_SIZE1:          return "Size1";
        case GG_COAP_MESSAGE_OPTION_SIZE2:          return "Size2";
        case GG_COAP_MESSAGE_OPTION_BLOCK1:         return "Block1";
        case GG_COAP_MESSAGE_OPTION_BLOCK2:         return "Block2";
        case GG_COAP_MESSAGE_OPTION_START_OFFSET:   return "Start-Offset";
        case GG_COAP_MESSAGE_OPTION_EXTENDED_ERROR: return "Extended-Error";
        default:                                    return "";
    }
}

//----------------------------------------------------------------------
static void
DumpResponse(GG_CoapMessage* message)
{
    size_t         payload_size = GG_CoapMessage_GetPayloadSize(message);
    const uint8_t* payload      = GG_CoapMessage_GetPayload(message);

    if (hex_dump) {
        for (unsigned int i = 0; i < payload_size; i++) {
            console_printf("%02X", payload[i]);
        }
        console_printf("\n");

        return;
    }

    uint8_t token[GG_COAP_MESSGAGE_MAX_TOKEN_LENGTH];
    size_t  token_length = GG_CoapMessage_GetToken(message, token);
    char token_hex[2*GG_COAP_MESSGAGE_MAX_TOKEN_LENGTH+1];
    GG_BytesToHex(token, token_length, token_hex, true);
    token_hex[2*token_length] = 0;
    console_printf("  token = %s\n", token_hex);

    GG_CoapMessageOptionIterator option_iterator;
    GG_CoapMessage_InitOptionIterator(message, GG_COAP_MESSAGE_OPTION_ITERATOR_FILTER_ANY, &option_iterator);

    while (option_iterator.option.number) {
        switch (option_iterator.option.type) {
            case GG_COAP_MESSAGE_OPTION_TYPE_UINT:
                console_printf("  option %u [%s] (uint): %u\n",
                               (int)option_iterator.option.number,
                               GetOptionName(option_iterator.option.number),
                               (int)option_iterator.option.value.uint);
                break;

            case GG_COAP_MESSAGE_OPTION_TYPE_STRING:
                console_printf("  option %u [%s] (string): %.*s\n",
                               (int)option_iterator.option.number,
                               GetOptionName(option_iterator.option.number),
                               (int)option_iterator.option.value.string.length,
                               option_iterator.option.value.string.chars);
                break;

            case GG_COAP_MESSAGE_OPTION_TYPE_OPAQUE:
                console_printf("  option %u [%s] (opaque): size=%u\n",
                               (int)option_iterator.option.number,
                               GetOptionName(option_iterator.option.number),
                               (int)option_iterator.option.value.opaque.size);
                break;

            case GG_COAP_MESSAGE_OPTION_TYPE_EMPTY:
                console_printf("  option %u [%s] (empty)\n",
                               (int)option_iterator.option.number,
                               GetOptionName(option_iterator.option.number));
                break;
        }

        GG_CoapMessage_StepOptionIterator(message, &option_iterator);
    }

    console_printf("  payload size = %d\n", (int)payload_size);
    if (payload_size == 0) {
        return;
    }

    console_printf("  payload:\n");
    for (unsigned int i = 0;
         i < (payload_size + GG_COAP_CLIENT_DUMP_CHUNK_SIZE - 1)/ GG_COAP_CLIENT_DUMP_CHUNK_SIZE;
         i++) {
        char hex_buffer[(2 * GG_COAP_CLIENT_DUMP_CHUNK_SIZE) + 1];
        char str_buffer[GG_COAP_CLIENT_DUMP_CHUNK_SIZE + 1];

        memset(hex_buffer, ' ', sizeof(hex_buffer) - 1);
        memset(str_buffer, ' ', sizeof(str_buffer) - 1);

        size_t chunk = GG_COAP_CLIENT_DUMP_CHUNK_SIZE;
        if (i * GG_COAP_CLIENT_DUMP_CHUNK_SIZE + chunk > payload_size) {
            chunk = payload_size - (i * GG_COAP_CLIENT_DUMP_CHUNK_SIZE);
        }

        GG_BytesToHex(&payload[i * GG_COAP_CLIENT_DUMP_CHUNK_SIZE], chunk, hex_buffer, true);
        for (unsigned int j = 0; j < chunk; j++) {
            uint8_t c = payload[i * GG_COAP_CLIENT_DUMP_CHUNK_SIZE + j];

            if (c >= 0x20 && c <= 0x7E) {
                str_buffer[j] = (char)c;
            } else {
                str_buffer[j] = '.';
            }
        }

        str_buffer[sizeof(str_buffer) - 1] = '\0';
        hex_buffer[sizeof(hex_buffer) - 1] = '\0';
        console_printf("  %04d: %s    %s\n", i * GG_COAP_CLIENT_DUMP_CHUNK_SIZE, str_buffer, hex_buffer);
    }
}

//----------------------------------------------------------------------
static void
CoapClient_OnBlockwiseError(GG_CoapBlockwiseResponseListener* _self, GG_Result error,
                            const char* message)
{
    GG_COMPILER_UNUSED(_self);

    console_printf("ERROR: error=%d, message=%s\n", error, message ? message : "");

    CoapClient_Cleanup();
}

//----------------------------------------------------------------------
// Invoked when a blockwise response is received
//----------------------------------------------------------------------
static void
CoapClient_OnResponseBlock(GG_CoapBlockwiseResponseListener* _self,
                           GG_CoapMessageBlockInfo*          block_info,
                           GG_CoapMessage*                   block_message)
{
    CoapClient* self = GG_SELF(CoapClient, GG_CoapBlockwiseResponseListener);

    if (verbose) {
        console_printf("=== Received response block, offset=%d:\n", (int)block_info->offset);
        unsigned int code = GG_CoapMessage_GetCode(block_message);
        console_printf("  code = %d.%02d\n", GG_COAP_MESSAGE_CODE_CLASS(code), GG_COAP_MESSAGE_CODE_DETAIL(code));
    }

    // check the block offset
    if (block_info->offset != self->expected_offset) {
        console_printf("WARNING: unexpected block offset\n");
    }

    // print info about the block
    DumpResponse(block_message);

    // check if we're done
    if (!block_info->more) {
        if (verbose) {
            console_printf("### Last block, we're done!\n");
        }

        CoapClient_Cleanup();
    }

    // update the expected next block offset
    self->expected_offset += GG_CoapMessage_GetPayloadSize(block_message);
}

//----------------------------------------------------------------------
static void
CoapClient_OnSimpleError(GG_CoapResponseListener* _self, GG_Result error, const char* message)
{
    GG_COMPILER_UNUSED(_self);

    console_printf("ERROR: error=%d, message=%s\n", error, message ? message : "");

    CoapClient_Cleanup();
}

//----------------------------------------------------------------------
// Invoked when a simple (i.e non-blockwise) response is received
//----------------------------------------------------------------------
static void
CoapClient_OnResponse(GG_CoapResponseListener* _self,
                      GG_CoapMessage*          message)
{
    GG_COMPILER_UNUSED(_self);

    if (verbose) {
        console_printf("=== Received response, payload size = %d\n", (int)GG_CoapMessage_GetPayloadSize(message));
        unsigned int code = GG_CoapMessage_GetCode(message);
        console_printf("  code = %d.%02d\n", GG_COAP_MESSAGE_CODE_CLASS(code), GG_COAP_MESSAGE_CODE_DETAIL(code));
    }

    // print info about the block
    DumpResponse(message);

    CoapClient_Cleanup();
}

//----------------------------------------------------------------------
static void
CoapClient_OnAck(GG_CoapResponseListener* _self)
{
    GG_COMPILER_UNUSED(_self);
}

//----------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(CoapClient, GG_CoapResponseListener) {
    .OnAck      = CoapClient_OnAck,
    .OnResponse = CoapClient_OnResponse,
    .OnError    = CoapClient_OnSimpleError
};

//----------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(CoapClient, GG_CoapBlockwiseResponseListener) {
    .OnResponseBlock = CoapClient_OnResponseBlock,
    .OnError         = CoapClient_OnBlockwiseError
};

//----------------------------------------------------------------------
static GG_Result
SendRequest(GG_CoapEndpoint*                  endpoint,
            bool                              use_blockwise,
            GG_CoapResponseListener*          simple_listener,
            GG_CoapBlockwiseResponseListener* blockwise_listener,
            GG_CoapBlockSource*               payload_source,
            GG_CoapMethod                     method,
            char*                             path_and_query,
            GG_CoapMessageOptionParam*        request_options,
            size_t                            request_options_count,
            size_t                            preferred_block_size)
{
    // split the path part and the query part
    char* path = path_and_query;
    char* query = NULL;
    char* query_delimiter = strchr(path_and_query, '?');

    if (query_delimiter) {
        *query_delimiter = '\0';
        query = query_delimiter + 1;
    }

    // parse the path
    size_t path_options_count = GG_COAP_CLIENT_MAX_PATH_COMPONENTS;
    GG_CoapMessageOptionParam path_options[GG_COAP_CLIENT_MAX_PATH_COMPONENTS];
    GG_CoapMessageOptionParam* options = path_options;

    GG_Result result = GG_Coap_SplitPathOrQuery(path,
                                                '/',
                                                path_options,
                                                &path_options_count,
                                                GG_COAP_MESSAGE_OPTION_URI_PATH);
    if (GG_FAILED(result)) {
        console_printf("ERROR: GG_Coap_SplitPath returned %d\n", result);
        return result;
    }

    if (!path_options_count) {
        return GG_ERROR_INVALID_SYNTAX;
    }

    // link options
    path_options[path_options_count - 1].next = request_options;

    // parse the query
    size_t query_options_count = 0;
    GG_CoapMessageOptionParam query_options[GG_COAP_CLIENT_MAX_QUERY_COMPONENTS];

    if (query) {
        query_options_count = GG_COAP_CLIENT_MAX_QUERY_COMPONENTS;

        result = GG_Coap_SplitPathOrQuery(query,
                                          '&',
                                          query_options,
                                          &query_options_count,
                                          GG_COAP_MESSAGE_OPTION_URI_QUERY);

        if (GG_FAILED(result)) {
            console_printf("ERROR: GG_Coap_SplitPath returned %d\n", result);
            return result;
        }

        if (query_options_count) {
            // link query options
            query_options[query_options_count - 1].next = request_options;
            path_options[path_options_count - 1].next = query_options;
        }
    }

    // send the request
    GG_CoapRequestHandle handle;
    if (use_blockwise) {
        client.expected_offset = 0;

        result = GG_CoapEndpoint_SendBlockwiseRequest(endpoint,
                                                      method,
                                                      options,
                                                      (unsigned int)(path_options_count +
                                                                     query_options_count +
                                                                     request_options_count),
                                                      payload_source,
                                                      preferred_block_size,
                                                      NULL,
                                                      blockwise_listener,
                                                      &handle);
    } else {
        // load the payload into a buffer, up to GG_COAP_MAX_SIMPLE_REQUEST_PAYLOAD bytes
        uint8_t payload[GG_COAP_MAX_SIMPLE_REQUEST_PAYLOAD];
        size_t payload_size = 0;

        if (payload_source) {
            payload_size = GG_COAP_MAX_SIMPLE_REQUEST_PAYLOAD;
            bool more = false;

            result = GG_CoapBlockSource_GetDataSize(payload_source, 0, &payload_size, &more);
            if (GG_FAILED(result)) {
                return result;
            }

            if (more) {
                console_printf("WARNING: payload is larger than %d\n", GG_COAP_MAX_SIMPLE_REQUEST_PAYLOAD);
            }

            result = GG_CoapBlockSource_GetData(payload_source, 0, payload_size, payload);
            if (GG_FAILED(result)) {
                return result;
            }
        }

        result = GG_CoapEndpoint_SendRequest(endpoint,
                                             method,
                                             options,
                                             (unsigned int)(path_options_count +
                                                            query_options_count +
                                                            request_options_count),
                                             payload_size ? payload : NULL,
                                             payload_size,
                                             NULL,
                                             simple_listener,
                                             &handle);
    }

    return result;
}

//----------------------------------------------------------------------
static void
CoapClient_Cleanup(void)
{
    MemSource_Deinit(&mem_source);

    memset(&request_info, 0, sizeof(request_info));

    verbose = false;
    hex_dump = false;
}

//----------------------------------------------------------------------
static void
SendRequest_Handle(void *arg)
{
    CoapRequest *req = (CoapRequest*)arg;
    GG_Result result;

    result = SendRequest(client.endpoint,
                         req->use_blockwise,
                         GG_CAST(&client, GG_CoapResponseListener),
                         GG_CAST(&client, GG_CoapBlockwiseResponseListener),
                         req->payload_source,
                         req->method,
                         req->uri,
                         req->request_options,
                         req->request_options_count,
                         req->preferred_block_size);

    if (GG_FAILED(result)) {
       console_printf("ERROR: failed send CoAP request");
    }
}

//----------------------------------------------------------------------
//   Parse an option argument of the form '<name>=<value>'
//----------------------------------------------------------------------
static GG_Result
ParseOption(const char* option, GG_CoapMessageOptionParam* option_param)
{
    if (!strncmp(option, "Content-Format=", 15)) {
        option_param->option.number = GG_COAP_MESSAGE_OPTION_CONTENT_FORMAT;
        option_param->option.type   = GG_COAP_MESSAGE_OPTION_TYPE_UINT;
        option += 15;
    } else if (!strncmp(option, "Block1=", 7)) {
        option_param->option.number = GG_COAP_MESSAGE_OPTION_BLOCK1;
        option_param->option.type   = GG_COAP_MESSAGE_OPTION_TYPE_UINT;
        option += 7;
    } else if (!strncmp(option, "Block2=", 7)) {
        option_param->option.number = GG_COAP_MESSAGE_OPTION_BLOCK2;
        option_param->option.type   = GG_COAP_MESSAGE_OPTION_TYPE_UINT;
        option += 7;
    } else if (!strncmp(option, "Size1=", 6)) {
        option_param->option.number = GG_COAP_MESSAGE_OPTION_SIZE1;
        option_param->option.type   = GG_COAP_MESSAGE_OPTION_TYPE_UINT;
        option += 6;
    } else if (!strncmp(option, "Size2=", 6)) {
        option_param->option.number = GG_COAP_MESSAGE_OPTION_SIZE2;
        option_param->option.type   = GG_COAP_MESSAGE_OPTION_TYPE_UINT;
        option += 6;
    } else if (!strncmp(option, "Start-Offset=", 13)) {
        option_param->option.number = GG_COAP_MESSAGE_OPTION_START_OFFSET;
        option_param->option.type   = GG_COAP_MESSAGE_OPTION_TYPE_UINT;
        option += 13;
    } else if (!strncmp(option, "Extended-Error=", 15)) {
        option_param->option.number = GG_COAP_MESSAGE_OPTION_EXTENDED_ERROR;
        option_param->option.type   = GG_COAP_MESSAGE_OPTION_TYPE_OPAQUE;
        option += 15;
    } else {
        return GG_ERROR_NOT_SUPPORTED;
    }

    // parse the value
    if (option_param->option.type == GG_COAP_MESSAGE_OPTION_TYPE_UINT) {
        option_param->option.value.uint = (uint32_t)strtoul(option, NULL, 10);
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
static void
PrintUsage(void)
{
    console_printf("gg coap/client payload reset [<buffer_size>]\n"
                   "  where buffer_size is max size in bytes of payload data to be sent\n"
                   "\n"
                   "gg coap/client payload append <hex_string>\n"
                   "  where hex_string is a string of hex chars to be converted into bytes\n"
                   "  and appended to the payload buffer\n"
                   "\n"
                   "gg coap/client get|put|post|delete [options] <uri>\n"
                   "  were URI must be of the form: <path>[?<query>]\n"
                   "\n"
                   "options:\n"
                   "  -v : verbose (print trace info)\n"
                   "  -x : dump responses only as hex strings\n"
                   "  -p <payload-data> : string with the the payload to put/post;\n"
                   "     This method overwrites data set using gg coap/client payload\n"
                   "  -o <option>=<value> (supported options: 'Content-Format=<uint>',\n"
                   "     Block1=<uint>, Block2=<uint>, Size1=<uint>, Size2=<uint>)\n"
                   "  -b <preferred-block-size> (16, 32, 64, 128, 256, 512 or 1024 for block-wise)\n"
                   "     transfers, or 0 to force a non-blockwise transfer\n");
}

//----------------------------------------------------------------------
static int
CoapClient_CLI_Payload(int argc, char** argv)
{
    GG_Result rc = GG_ERROR_INVALID_PARAMETERS;

    if (!strcmp(argv[1], "reset") && (argc  == 2 || argc == 3)) {
        size_t buf_size = argc == 3 ? (size_t)atoi(argv[2]) : 0;

        rc = MemSource_ResetData(&mem_source, buf_size);

        if (GG_FAILED(rc)) {
            console_printf("Failed to allocate payload buffer!\n");
        }
    } else if (!strcmp(argv[1], "append") && argc == 3) {
        rc = MemSource_AppendHexString(&mem_source, argv[2], strlen(argv[2]));

        if (GG_FAILED(rc)) {
            console_printf("Failed to append payload to buffer!\n");
        }
    } else {
        PrintUsage();
    }

    return GG_SUCCEEDED(rc) ? 0 : 1;
}


//----------------------------------------------------------------------
GG_Result
CoapClient_Init(GG_Loop* loop, GG_CoapEndpoint* endpoint)
{
    client.loop     = loop;
    client.endpoint = endpoint;

    GG_SET_INTERFACE(&client, CoapClient, GG_CoapResponseListener);
    GG_SET_INTERFACE(&client, CoapClient, GG_CoapBlockwiseResponseListener);

    GG_SET_INTERFACE(&mem_source, MemSource, GG_CoapBlockSource);

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
int
CoapClient_CLI_Handler(int argc, char** argv)
{
    GG_CoapMessageOptionParam request_options[GG_COAP_CLIENT_MAX_OPTIONS_COUNT];
    size_t                    request_options_count = 0;
    GG_CoapMethod             method;
    char*                     uri = NULL;
    const char*               payload_data = NULL;
    size_t                    preferred_block_size = 0;
    bool                      use_blockwise = true;
    GG_Result                 result;

    // parse the command line arguments
    if (argc < 3) {
        PrintUsage();
        return 1;
    }

    ++argv; // skip first arg

    const char* method_string = *argv;

    if (!strcmp(method_string, "get")) {
        method = GG_COAP_METHOD_GET;
    } else if (!strcmp(method_string, "put")) {
        method = GG_COAP_METHOD_PUT;
    } else if (!strcmp(method_string, "post")) {
        method = GG_COAP_METHOD_POST;
    } else if (!strcmp(method_string, "delete")) {
        method = GG_COAP_METHOD_DELETE;
    } else if (!strcmp(method_string, "payload")) {
        return CoapClient_CLI_Payload(argc - 1, argv);
    } else {
        console_printf("ERROR: invalid method %s\n", method_string);
        return 1;
    }

    ++argv;  // skip method

    char* arg;

    while ((arg = *argv++)) {
        if (!strcmp(arg, "-p")) {
            payload_data = *argv++;
            if (!payload_data) {
                console_printf("ERROR: missing parameter after -p option\n");
                return 1;
            }
        } else if (!strcmp(arg, "-o")) {
            char* option = *argv++;

            if (!option) {
                console_printf("ERROR: missing parameter after -o option\n");
                return 1;
            }

            result = ParseOption(option, &request_options[request_options_count]);
            if (GG_FAILED(result)) {
                console_printf("ERROR: unsupported or invalid option\n");
                return 1;
            }

            ++request_options_count;
        } else if (!strcmp(arg, "-b")) {
            char* size = *argv++;

            if (!size) {
                console_printf("ERROR: missing parameter after -b option\n");
                return 1;
            }

            preferred_block_size = strtoul(size, NULL, 10);
            if (preferred_block_size != 0   &&
                preferred_block_size != 16  &&
                preferred_block_size != 32  &&
                preferred_block_size != 64  &&
                preferred_block_size != 128 &&
                preferred_block_size != 256 &&
                preferred_block_size != 512 &&
                preferred_block_size != 1024) {
                console_printf("ERROR: invalid block size\n");
                return 1;
            }

            if (preferred_block_size == 0) {
                use_blockwise = false;
            }
        } else if (!strcmp(arg, "-v")) {
            verbose = true;
        } else if (!strcmp(arg, "-x")) {
            hex_dump = true;
        } else {
            if (uri) {
                console_printf("ERROR: unexpected argument '%s'\n", arg);
                return 1;
            } else {
                uri = arg;
            }
        }
    }

    if (!uri) {
        console_printf("ERROR: missing uri argument\n");
        return 1;
    }

    // setup a payload source if needed
    GG_CoapBlockSource* payload_source = NULL;
    if (payload_data) {
        result = MemSource_SetData(&mem_source, (uint8_t *)payload_data, strlen(payload_data));
        if (GG_FAILED(result)) {
            console_printf("ERROR: failed to set payload (%d)\n", result);
            return 1;
        }
    }

    if (MemSource_GetPayloadSize(&mem_source) != 0) {
        payload_source = GG_CAST(&mem_source, GG_CoapBlockSource);
    }

    // make the request
    request_info.use_blockwise = use_blockwise;
    request_info.payload_source = payload_source;
    request_info.method = method;
    request_info.uri = uri;
    request_info.request_options = request_options;
    request_info.request_options_count = request_options_count;
    request_info.preferred_block_size = preferred_block_size;

    result = GG_Loop_InvokeAsync(client.loop, SendRequest_Handle, &request_info);

    return result == GG_SUCCESS ? 0 : 1;
}

/**
 *
 * @file
 *
 * @copyright
 * Copyright 2017 by Fitbit, Inc., all rights reserved.
 *
 * @author Gilles Boccon-Gibod
 *
 * @date 2017-09-18
 *
 * @details
 *
 * Simple CoAP client command line application
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "xp/common/gg_utils.h"
#include "xp/common/gg_port.h"
#include "xp/common/gg_memory.h"
#include "xp/sockets/gg_sockets.h"
#include "xp/sockets/ports/bsd/gg_bsd_sockets.h"
#include "xp/module/gg_module.h"
#include "xp/coap/gg_coap.h"
#include "xp/coap/gg_coap_blockwise.h"

/*----------------------------------------------------------------------
|   globals
+---------------------------------------------------------------------*/
typedef struct {
    GG_IMPLEMENTS(GG_CoapResponseListener);
    GG_IMPLEMENTS(GG_CoapBlockwiseResponseListener);

    GG_DatagramSocket* socket;
    GG_CoapEndpoint*   endpoint;
    size_t             expected_offset;
    FILE*              output_file;
} Client;

typedef struct {
    GG_IMPLEMENTS(GG_CoapBlockSource);

    uint8_t* data;
    size_t   data_size;
} MemSource;

static GG_Loop* Loop;

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/
#define GG_COAP_CLIENT_MAX_PATH_COMPONENTS  16
#define GG_COAP_CLIENT_MAX_QUERY_COMPONENTS 16
#define GG_COAP_CLIENT_MAX_OPTIONS_COUNT    16
#define GG_COAP_CLIENT_DUMP_CHUNK_SIZE      16
#define GG_COAP_CLIENT_MTU                  1280
#define GG_COAP_URI_PREFIX                  "coap://"
#define GG_COAP_MAX_SIMPLE_REQUEST_PAYLOAD  1024

/*----------------------------------------------------------------------
|   globals
+---------------------------------------------------------------------*/
static bool Quiet;

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

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
    uint8_t token[GG_COAP_MESSGAGE_MAX_TOKEN_LENGTH];
    size_t  token_length = GG_CoapMessage_GetToken(message, token);
    char token_hex[2*GG_COAP_MESSGAGE_MAX_TOKEN_LENGTH+1];
    GG_BytesToHex(token, token_length, token_hex, true);
    token_hex[2*token_length] = 0;
    fprintf(stderr, "  token = %s\n", token_hex);

    GG_CoapMessageOptionIterator option_iterator;
    GG_CoapMessage_InitOptionIterator(message, GG_COAP_MESSAGE_OPTION_ITERATOR_FILTER_ANY, &option_iterator);
    while (option_iterator.option.number) {
        switch (option_iterator.option.type) {
            case GG_COAP_MESSAGE_OPTION_TYPE_UINT:
                fprintf(stderr, "  option %u [%s] (uint): %u\n",
                        (int)option_iterator.option.number,
                        GetOptionName(option_iterator.option.number),
                        (int)option_iterator.option.value.uint);
                break;

            case GG_COAP_MESSAGE_OPTION_TYPE_STRING:
                fprintf(stderr, "  option %u [%s] (string): %.*s\n",
                        (int)option_iterator.option.number,
                        GetOptionName(option_iterator.option.number),
                        (int)option_iterator.option.value.string.length,
                        option_iterator.option.value.string.chars);
                break;

            case GG_COAP_MESSAGE_OPTION_TYPE_OPAQUE:
                fprintf(stderr, "  option %u [%s] (opaque): size=%u",
                        (int)option_iterator.option.number,
                        GetOptionName(option_iterator.option.number),
                        (int)option_iterator.option.value.opaque.size);

                fprintf(stderr, " value=");
                for (size_t i = 0; i < option_iterator.option.value.opaque.size; i++) {
                    fprintf(stderr, "%02x",
                            option_iterator.option.value.opaque.bytes[i]);
                }
                fprintf(stderr, "\n");
                break;

            case GG_COAP_MESSAGE_OPTION_TYPE_EMPTY:
                fprintf(stderr, "  option %u [%s] (empty)\n",
                        (int)option_iterator.option.number,
                        GetOptionName(option_iterator.option.number));
                break;
        }

        GG_CoapMessage_StepOptionIterator(message, &option_iterator);
    }

    size_t         payload_size = GG_CoapMessage_GetPayloadSize(message);
    const uint8_t* payload      = GG_CoapMessage_GetPayload(message);
    fprintf(stderr, "  payload size = %d\n", (int)payload_size);
    if (payload_size == 0) {
        return;
    }

    fprintf(stderr, "  payload:\n");
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
        fprintf(stderr, "  %04d: %s    %s\n", i * GG_COAP_CLIENT_DUMP_CHUNK_SIZE, str_buffer, hex_buffer);
    }
}

//----------------------------------------------------------------------
static void
Client_OnBlockwiseError(GG_CoapBlockwiseResponseListener* _self, GG_Result error, const char* message)
{
    GG_COMPILER_UNUSED(_self);

    fprintf(stderr, "ERROR: error=%d, message=%s\n", error, message ? message : "");

    GG_Loop_RequestTermination(Loop);
}

//----------------------------------------------------------------------
// Invoked when a blockwise response is received
//----------------------------------------------------------------------
static void
Client_OnResponseBlock(GG_CoapBlockwiseResponseListener* _self,
                       GG_CoapMessageBlockInfo*          block_info,
                       GG_CoapMessage*                   block_message)
{
    Client* self = GG_SELF(Client, GG_CoapBlockwiseResponseListener);

    if (!Quiet) {
        fprintf(stderr, "=== Received response block, offset=%d:\n", (int)block_info->offset);
        unsigned int code = GG_CoapMessage_GetCode(block_message);
        fprintf(stderr, "  code = %d.%02d\n", GG_COAP_MESSAGE_CODE_CLASS(code), GG_COAP_MESSAGE_CODE_DETAIL(code));
    }

    // check the block offset
    if (block_info->offset != self->expected_offset) {
        fprintf(stderr, "WARNING: unexpected block offset\n");
    }

    // print info about the block
    if (!Quiet) {
        DumpResponse(block_message);
    }

    // output the block
    fwrite(GG_CoapMessage_GetPayload(block_message),
           GG_CoapMessage_GetPayloadSize(block_message),
           1,
           self->output_file);

    // check if we're done
    if (!block_info->more) {
        if (!Quiet) {
            fprintf(stderr, "### Last block, we're done!\n");
        }
        GG_Loop_RequestTermination(Loop);
    }

    // update the expected next block offset
    self->expected_offset += GG_CoapMessage_GetPayloadSize(block_message);
}

//----------------------------------------------------------------------
static void
Client_OnSimpleError(GG_CoapResponseListener* _self, GG_Result error, const char* message)
{
    GG_COMPILER_UNUSED(_self);

    fprintf(stderr, "ERROR: error=%d, message=%s\n", error, message ? message : "");

    GG_Loop_RequestTermination(Loop);
}

//----------------------------------------------------------------------
// Invoked when a simple (i.e non-blockwise) response is received
//----------------------------------------------------------------------
static void
Client_OnResponse(GG_CoapResponseListener* _self,
                  GG_CoapMessage*          message)
{
    Client* self = GG_SELF(Client, GG_CoapResponseListener);

    if (!Quiet) {
        fprintf(stderr, "=== Received response, payload size = %d\n", (int)GG_CoapMessage_GetPayloadSize(message));
        unsigned int code = GG_CoapMessage_GetCode(message);
        fprintf(stderr, "  code = %d.%02d\n", GG_COAP_MESSAGE_CODE_CLASS(code), GG_COAP_MESSAGE_CODE_DETAIL(code));
    }

    // print info about the block
    if (!Quiet) {
        DumpResponse(message);
    }

    // output the block
    fwrite(GG_CoapMessage_GetPayload(message), GG_CoapMessage_GetPayloadSize(message), 1, self->output_file);

    // we're done
    GG_Loop_RequestTermination(Loop);
}

//----------------------------------------------------------------------
static void
Client_OnAck(GG_CoapResponseListener* _self)
{
    GG_COMPILER_UNUSED(_self);
}

//----------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(Client, GG_CoapResponseListener) {
    .OnAck      = Client_OnAck,
    .OnResponse = Client_OnResponse,
    .OnError    = Client_OnSimpleError
};

//----------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(Client, GG_CoapBlockwiseResponseListener) {
    .OnResponseBlock = Client_OnResponseBlock,
    .OnError         = Client_OnBlockwiseError
};

//----------------------------------------------------------------------
static GG_Result
MemSource_GetDataSize(GG_CoapBlockSource* _self,
                      size_t              offset,
                      size_t*             data_size,
                      bool*               more)
{
    MemSource* self = GG_SELF(MemSource, GG_CoapBlockSource);

    return GG_CoapMessageBlockInfo_AdjustAndGetChunkSize(offset, data_size, more, self->data_size);
}

//----------------------------------------------------------------------
static GG_Result
MemSource_GetData(GG_CoapBlockSource* _self,
                  size_t              offset,
                  size_t              data_size,
                  void*               data)
{
    MemSource* self = GG_SELF(MemSource, GG_CoapBlockSource);

    if (offset + data_size <= self->data_size) {
        memcpy(data, self->data + offset, data_size);
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
static GG_Result
MemSource_Init(MemSource* self, const char* filename)
{
    FILE* file = fopen(filename, "rb");
    if (!file) {
        return GG_FAILURE;
    }

    GG_SET_INTERFACE(self, MemSource, GG_CoapBlockSource);

    // measure the size by seeking
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    self->data_size = (size_t)size;
    fseek(file, 0, SEEK_SET);

    // allocate the data
    self->data = GG_AllocateMemory(self->data_size);
    if (!self->data) {
        return GG_ERROR_OUT_OF_MEMORY;
    }

    // read the data
    size_t result = fread(self->data, self->data_size, 1, file);
    if (result != 1) {
        GG_FreeMemory(self->data);
        fclose(file);
        return GG_FAILURE;
    }

    fclose(file);
    return GG_SUCCESS;
}

//----------------------------------------------------------------------
//   Parse the host:port part of the URI, create the socket for it and
//   create and endpoint attached to the socket. Update the URI string
//   to point to the URI path.
//----------------------------------------------------------------------
static bool
CreateEndpoint(char** host_and_path, GG_CoapEndpoint** endpoint, GG_DatagramSocket** socket)
{
    *endpoint = NULL;
    *socket   = NULL;

    // get the host and port
    GG_SocketAddress remote_address = (GG_SocketAddress) {
        .address = GG_IP_ADDRESS_NULL_INITIALIZER,
        .port    = GG_COAP_DEFAULT_PORT
    };
    char* delimiter = *host_and_path;
    while (*delimiter && *delimiter != '/') {
        ++delimiter;
    }
    if (*delimiter != '/') {
        fprintf(stderr, "ERROR: invalid URI\n");
        return GG_ERROR_INVALID_SYNTAX;
    }
    *delimiter = '\0';
    char* path = delimiter + 1;
    char* colon = strchr(*host_and_path, ':');
    if (colon) {
        *colon = '\0';
        unsigned long port = strtoul(colon + 1, NULL, 10);
        if (port == 0 || port > 65535) {
            fprintf(stderr, "ERROR: invalid port\n");
            return GG_ERROR_INVALID_SYNTAX;
        }
        remote_address.port = (uint16_t)port;
    }
    if (GG_FAILED(GG_IpAddress_SetFromString(&remote_address.address, *host_and_path))) {
        fprintf(stderr, "ERROR: invalid IP address\n");
        return GG_ERROR_INVALID_SYNTAX;
    }

    // create a socket to communicate
    GG_Result result = GG_BsdDatagramSocket_Create(NULL,
                                                   &remote_address,
                                                   true,
                                                   GG_COAP_CLIENT_MTU,
                                                   socket);
    if (GG_FAILED(result)) {
        return result;
    }
    GG_DatagramSocket_Attach(*socket, Loop);

    // create an endpoint
    result = GG_CoapEndpoint_Create(GG_Loop_GetTimerScheduler(Loop),
                                    GG_DatagramSocket_AsDataSink(*socket),
                                    GG_DatagramSocket_AsDataSource(*socket),
                                    endpoint);
    if (GG_FAILED(result)) {
        GG_DatagramSocket_Destroy(*socket);
        return result;
    }

    // update the URI to point to the path
    *host_and_path = path;

    return GG_SUCCESS;
}

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
            size_t                            preferred_block_size,
            const GG_CoapClientParameters*    client_parameters)
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
        fprintf(stderr, "GG_Coap_SplitPath returned %d\n", result);
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
            fprintf(stderr, "GG_Coap_SplitPath returned %d\n", result);
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
        result = GG_CoapEndpoint_SendBlockwiseRequest(endpoint,
                                                      method,
                                                      options,
                                                      (unsigned int)(path_options_count +
                                                                     query_options_count +
                                                                     request_options_count),
                                                      payload_source,
                                                      preferred_block_size,
                                                      client_parameters,
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
                fprintf(stderr, "WARNING: payload is larger than %d\n", GG_COAP_MAX_SIMPLE_REQUEST_PAYLOAD);
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
                                             client_parameters,
                                             simple_listener,
                                             &handle);
    }

    return result;
}

//----------------------------------------------------------------------
//   Parse an option argument of the form '<name>=<value>'
//----------------------------------------------------------------------
static GG_Result
ParseOption(const char* option, GG_CoapMessageOptionParam* option_param)
{
    static uint8_t opaque_storage[256];
    static size_t  opaque_storage_used = 0;

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
    } else if (!strncmp(option, "If-Match=", 9)) {
        option_param->option.number = GG_COAP_MESSAGE_OPTION_IF_MATCH;
        option_param->option.type   = GG_COAP_MESSAGE_OPTION_TYPE_OPAQUE;
        option += 9;
    } else {
        return GG_ERROR_NOT_SUPPORTED;
    }

    // parse the value
    if (option_param->option.type == GG_COAP_MESSAGE_OPTION_TYPE_UINT) {
        option_param->option.value.uint = (uint32_t)strtoul(option, NULL, 10);
    } else if (option_param->option.type == GG_COAP_MESSAGE_OPTION_TYPE_OPAQUE) {
        size_t hex_length = strlen(option);
        if ((hex_length % 2) || (hex_length / 2) > (sizeof(opaque_storage) - opaque_storage_used)) {
            fprintf(stderr, "ERROR: cannot accept opaque option\n");
            return GG_ERROR_INVALID_SYNTAX;
        }
        option_param->option.value.opaque.bytes = &opaque_storage[opaque_storage_used];
        option_param->option.value.opaque.size  = hex_length / 2;
        GG_HexToBytes(option, hex_length, &opaque_storage[opaque_storage_used]);
        opaque_storage_used += hex_length / 2;
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
static void
PrintUsage(void)
{
    printf("gg-coap-client get|put|post|delete [options] <uri>\n"
           "  were URI must be of the form: coap://<ipv4-addr>[:port]/<path>[?<query>]\n"
           "\n"
           "options:\n"
           "  -q : be quiet (don't print out log/trace information)\n"
           "  -p <payload-filename> : name of the file containing the payload to put/post\n"
           "  -o <option>=<value> (supported options: 'Content-Format=<uint>',\n"
           "   Block1=<uint>, Block2=<uint>, Size1=<uint>, Size2=<uint>, Start-Offset=<uint>,\n"
           "   If-Match=<opaque-hex>)\n"
           "  -b <preferred-block-size> (16, 32, 64, 128, 256, 512 or 1024 for block-wise)\n"
           "     transfers, or 0 to force a non-blockwise transfer\n"
           "  -s <filename> : save the response payload to <filename>\n"
           "  -t <ack-timeout> : response ACK timeout, in milliseconds\n"
           "  -r <max-resend-count>: maximum number of resends upon request timeouts\n");
}

//----------------------------------------------------------------------
int
main(int argc, char** argv)
{
    // parse the command line
    if (argc < 3) {
        PrintUsage();
        return 1;
    }

    // initialize Golden Gate
    GG_Module_Initialize();

    GG_CoapMessageOptionParam request_options[GG_COAP_CLIENT_MAX_OPTIONS_COUNT];
    size_t                    request_options_count = 0;
    GG_CoapMethod             method;
    char*                     uri = NULL;
    const char*               payload_filename = NULL;
    size_t                    preferred_block_size = 0;
    bool                      use_blockwise = true;
    FILE*                     output_file = stdout;
    const char*               output_filename = NULL;
    GG_CoapClientParameters   client_parameters = {
        .ack_timeout      = 0, // use defaults
        .max_resend_count = GG_COAP_DEFAULT_MAX_RETRANSMIT
    };
    bool                      use_client_parameters = false;
    GG_Result                 result;

    // parse the command line arguments
    ++argv; // skip first arg
    const char* method_string = *argv++;
    if (!strcmp(method_string, "get")) {
        method = GG_COAP_METHOD_GET;
    } else if (!strcmp(method_string, "put")) {
        method = GG_COAP_METHOD_PUT;
    } else if (!strcmp(method_string, "post")) {
        method = GG_COAP_METHOD_POST;
    } else if (!strcmp(method_string, "delete")) {
        method = GG_COAP_METHOD_DELETE;
    } else {
        fprintf(stderr, "ERROR: invalid method %s\n", method_string);
        return 1;
    }

    char* arg;
    while ((arg = *argv++)) {
        if (!strcmp(arg, "-p")) {
            payload_filename = *argv++;
            if (!payload_filename) {
                fprintf(stderr, "ERROR: missing parameter after -p option\n");
                return 1;
            }
        } else if (!strcmp(arg, "-o")) {
            char* option = *argv++;
            if (!option) {
                fprintf(stderr, "ERROR: missing parameter after -o option\n");
                return 1;
            }
            result = ParseOption(option, &request_options[request_options_count]);
            if (GG_FAILED(result)) {
                fprintf(stderr, "ERROR: unsupported or invalid option\n");
                return 1;
            }
            ++request_options_count;
        } else if (!strcmp(arg, "-b")) {
            char* size = *argv++;
            if (!size) {
                fprintf(stderr, "ERROR: missing parameter after -b option\n");
                return 1;
            }
            preferred_block_size = strtoul(size, NULL, 10);
            if (preferred_block_size !=  0  &&
                preferred_block_size != 16  &&
                preferred_block_size != 32  &&
                preferred_block_size != 64  &&
                preferred_block_size != 128 &&
                preferred_block_size != 256 &&
                preferred_block_size != 512 &&
                preferred_block_size != 1024) {
                fprintf(stderr, "ERROR: invalid block size\n");
                return 1;
            }
            if (preferred_block_size == 0) {
                use_blockwise = false;
            }
        } else if (!strcmp(arg, "-s")) {
            output_filename = *argv++;
            if (!output_filename) {
                fprintf(stderr, "ERROR: missing parameter after -s option\n");
                return 1;
            }
        } else if (!strcmp(arg, "-t")) {
            if (!*argv) {
                fprintf(stderr, "ERROR: missing timeout parameter after -t option\n");
            }
            client_parameters.ack_timeout = (uint32_t)strtoul(*argv++, NULL, 10);
            use_client_parameters = true;
        } else if (!strcmp(arg, "-r")) {
            if (!*argv) {
                fprintf(stderr, "ERROR: missing parameter after -r option\n");
            }
            client_parameters.max_resend_count = (size_t)strtoul(*argv++, NULL, 10);
            use_client_parameters = true;
        } else if (!strcmp(arg, "-q")) {
            Quiet = true;
        } else {
            if (uri) {
                fprintf(stderr, "ERROR: unexpected argument '%s'\n", arg);
                return 1;
            } else {
                uri = arg;
            }
        }
    }
    if (!uri) {
        fprintf(stderr, "ERROR: missing uri argument\n");
        return 1;
    }

    // check that the URI stars with 'coap://'
    if (strncmp(uri, GG_COAP_URI_PREFIX, strlen(GG_COAP_URI_PREFIX))) {
        fprintf(stderr, "ERROR: URI does not start with 'coap://'\n");
        return false;
    }

    // skip the scheme part
    uri += 7;

    // init a memory source
    MemSource mem_source;
    memset(&mem_source, 0, sizeof(MemSource));

    // setup a loop
    int exit_code = 1;
    result = GG_Loop_Create(&Loop);
    GG_ASSERT(result == GG_SUCCESS);
    result = GG_Loop_BindToCurrentThread(Loop);
    GG_ASSERT(result == GG_SUCCESS);

    // create and connect the endpoint
    GG_CoapEndpoint*   endpoint = NULL;
    GG_DatagramSocket* socket   = NULL;
    result = CreateEndpoint(&uri, &endpoint, &socket);
    if (GG_FAILED(result)) {
        fprintf(stderr, "ERROR: failed to create endpoint (%d)\n", result);
        goto end;
    }

    // setup a listener
    Client listener;
    memset(&listener, 0, sizeof(Client));
    listener.output_file = output_file;
    GG_SET_INTERFACE(&listener, Client, GG_CoapResponseListener);
    GG_SET_INTERFACE(&listener, Client, GG_CoapBlockwiseResponseListener);

    // setup a payload source if needed
    GG_CoapBlockSource* payload_source = NULL;
    if (payload_filename) {
        result = MemSource_Init(&mem_source, payload_filename);
        if (GG_FAILED(result)) {
            fprintf(stderr, "ERROR: failed to read payload file (%d)\n", result);
            goto end;
        }

        payload_source = GG_CAST(&mem_source, GG_CoapBlockSource);
    }

    // open the output file if needed
    if (output_filename) {
        output_file = fopen(output_filename, "wb+");
        if (!output_file) {
            fprintf(stderr, "ERROR: failed to open output file\n");
            return 1;
        }
    }

    // make the request
    result = SendRequest(endpoint,
                         use_blockwise,
                         GG_CAST(&listener, GG_CoapResponseListener),
                         GG_CAST(&listener, GG_CoapBlockwiseResponseListener),
                         payload_source,
                         method,
                         uri,
                         request_options,
                         request_options_count,
                         preferred_block_size,
                         use_client_parameters ? &client_parameters : NULL);
    if (GG_SUCCEEDED(result)) {
        // run the loop
        GG_Loop_Run(Loop);
        exit_code = 0;
    } else {
        fprintf(stderr, "ERROR: failed to send request (%d)\n", result);
    }

end:
    // cleanup
    if (output_filename && output_file) {
        fclose(output_file);
    }
    GG_CoapEndpoint_Destroy(endpoint);
    GG_DatagramSocket_Destroy(socket);
    if (mem_source.data) {
        GG_FreeMemory(mem_source.data);
    }
    GG_Loop_Destroy(Loop);
    GG_Module_Terminate();

    return exit_code;
}

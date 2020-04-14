/**
 *
 * @file
 *
 * @copyright
 * Copyright 2017-2018 by Fitbit, Inc., all rights reserved.
 *
 * @author Gilles Boccon-Gibod
 *
 * @date 2018-05-20
 *
 * @details
 *
 * Coap Blockwise Example application
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <inttypes.h>

#include "xp/common/gg_common.h"
#include "xp/sockets/gg_sockets.h"
#include "xp/sockets/ports/bsd/gg_bsd_sockets.h"
#include "xp/loop/gg_loop.h"
#include "xp/coap/gg_coap.h"
#include "xp/coap/gg_coap_blockwise.h"

//----------------------------------------------------------------------
// Globals
//----------------------------------------------------------------------
static GG_Loop* g_loop;

//----------------------------------------------------------------------
// CoAP payload source that returns a large payload
//----------------------------------------------------------------------
typedef struct {
    GG_IMPLEMENTS(GG_CoapBlockSource);

    size_t payload_size;
} BlockSource;

static GG_Result
BlockSource_GetDataSize(GG_CoapBlockSource* _self,
                        size_t              offset,
                        size_t*             data_size,
                        bool*               more)
{
    BlockSource* self = GG_SELF(BlockSource, GG_CoapBlockSource);

    return GG_CoapMessageBlockInfo_AdjustAndGetChunkSize(offset,
                                                         data_size,
                                                         more,
                                                         self->payload_size);
}

static GG_Result
BlockSource_GetData(GG_CoapBlockSource* _self,
                    size_t              offset,
                    size_t              data_size,
                    void*               data)
{
    // BlockSource* self = GG_SELF(BlockSource, GG_CoapBlockSource);
    GG_COMPILER_UNUSED(_self);

    // write a pattern
    uint8_t* payload = (uint8_t*)data;
    for (unsigned int i = 0; i < data_size; i++) {
        payload[i] = (uint8_t)('A' + (offset / data_size));
    }

    return GG_SUCCESS;
}

GG_IMPLEMENT_INTERFACE(BlockSource, GG_CoapBlockSource) {
    .GetDataSize = BlockSource_GetDataSize,
    .GetData     = BlockSource_GetData
};

//----------------------------------------------------------------------
// CoAP handler that accepts a large payload
//----------------------------------------------------------------------
typedef struct {
    GG_IMPLEMENTS(GG_CoapRequestHandler);

    GG_CoapBlockwiseServerHelper block1_helper;
    uint32_t                     session_id;
} Handler1;

static GG_Result
Handler1_OnRequest(GG_CoapRequestHandler*   _self,
                   GG_CoapEndpoint*         endpoint,
                   const GG_CoapMessage*    request,
                   GG_CoapResponder*        responder,
                   const GG_BufferMetadata* transport_metadata,
                   GG_CoapMessage**         response)
{
    Handler1* self = GG_SELF(Handler1, GG_CoapRequestHandler);
    GG_COMPILER_UNUSED(responder);
    GG_COMPILER_UNUSED(transport_metadata);

    bool request_was_resent = false;
    GG_Result result = GG_CoapBlockwiseServerHelper_OnRequest(&self->block1_helper,
                                                              request,
                                                              &request_was_resent);
    if (result != GG_SUCCESS) {
        printf("--- handler1 block error: %d\n", result);
        return result;
    }

    printf("--- handler1 received block %d@%d, more=%s (resent=%s)\n",
           (int)self->block1_helper.block_info.offset,
           (int)self->block1_helper.block_info.size,
           self->block1_helper.block_info.more ? "true" : "false",
           request_was_resent ? "true" : "false");

    // if this is the first block, start a new session
    if (self->block1_helper.block_info.offset == 0) {
        printf("--- handler 1 starting new session, ID=%u\n", (int)self->session_id);
        ++self->session_id;
        uint8_t etag[8];
        GG_GetRandomBytes(etag, 4); // 4 random bytes
        GG_BytesFromInt32Be(&etag[4], self->session_id); // followed by the session ID

        GG_CoapBlockwiseServerHelper_SetEtag(&self->block1_helper, etag, sizeof(etag));
    }

    return GG_CoapBlockwiseServerHelper_CreateResponse(&self->block1_helper,
                                                       endpoint,
                                                       request,
                                                       self->block1_helper.block_info.more ?
                                                       GG_COAP_MESSAGE_CODE_CONTINUE :
                                                       GG_COAP_MESSAGE_CODE_CHANGED,
                                                       NULL, 0,
                                                       NULL, 0,
                                                       response);
}

GG_IMPLEMENT_INTERFACE(Handler1, GG_CoapRequestHandler) {
    Handler1_OnRequest
};

static void
Handler1_Init(Handler1* self)
{
    GG_CoapBlockwiseServerHelper_Init(&self->block1_helper, GG_COAP_MESSAGE_OPTION_BLOCK1, 0);
    GG_SET_INTERFACE(self, Handler1, GG_CoapRequestHandler);
}

//----------------------------------------------------------------------
// CoAP handler that returns a large payload, possibly asynchronously
//----------------------------------------------------------------------
typedef struct {
    GG_IMPLEMENTS(GG_CoapRequestHandler);

    size_t       payload_size;
    unsigned int response_delay;
} Handler2;

typedef struct {
    GG_IMPLEMENTS(GG_TimerListener);

    GG_Timer*               timer;
    Handler2*               handler;
    GG_CoapEndpoint*        endpoint;
    const GG_CoapMessage*   request;
    GG_CoapResponder*       responder;
    GG_CoapMessageBlockInfo block;
} Handler2Responder;

static GG_CoapMessage*
Handler2_CreateResponse(Handler2*               self,
                        GG_CoapMessageBlockInfo block_info,
                        GG_CoapEndpoint*        endpoint,
                        const GG_CoapMessage*   request)
{
    GG_Result result;
    size_t chunk_size = block_info.size;
    result = GG_CoapMessageBlockInfo_AdjustAndGetChunkSize(block_info.offset,
                                                           &chunk_size,
                                                           &block_info.more,
                                                           self->payload_size);
    if (GG_FAILED(result)) {
        return NULL;
    }

    uint8_t payload[1024];
    for (unsigned int i = 0; i < chunk_size; i++) {
        payload[i] = (uint8_t)('A' + (block_info.offset / block_info.size));
    }

    GG_CoapMessage* response = NULL;
    result = GG_CoapEndpoint_CreateBlockwiseResponse(endpoint,
                                                     request,
                                                     GG_COAP_MESSAGE_CODE_CONTENT,
                                                     NULL, 0,
                                                     payload, chunk_size,
                                                     GG_COAP_MESSAGE_OPTION_BLOCK2,
                                                     &block_info,
                                                     &response);

    if (GG_FAILED(result)) {
        return NULL;
    }
    return response;
}

static void
Handler2Responder_Destroy(Handler2Responder* self)
{
    GG_Timer_Destroy(self->timer);
    GG_CoapResponder_Release(self->responder);
    GG_FreeMemory(self);
}

static void
Handler2Responder_OnTimerFired(GG_TimerListener* _self, GG_Timer* timer, uint32_t time_elapsed)
{
    Handler2Responder* self = GG_SELF(Handler2Responder, GG_TimerListener);
    GG_COMPILER_UNUSED(timer);
    GG_COMPILER_UNUSED(time_elapsed);

    printf("*** response timer fired\n");

    // create our response
    GG_CoapMessage* response = Handler2_CreateResponse(self->handler,
                                                       self->block,
                                                       self->endpoint,
                                                       self->request);
    GG_ASSERT(response);

    // send the response using the responder
    GG_CoapResponder_SendResponse(self->responder, response);

    // destroy the response
    GG_CoapMessage_Destroy(response);

    // we don't need to live anymore
    Handler2Responder_Destroy(self);
}

GG_IMPLEMENT_INTERFACE(Handler2Responder, GG_TimerListener) {
    Handler2Responder_OnTimerFired
};

static Handler2Responder*
Handler2Responder_Create(Handler2*               handler,
                         GG_CoapEndpoint*        endpoint,
                         const GG_CoapMessage*   request,
                         GG_CoapResponder*       responder,
                         GG_CoapMessageBlockInfo block)
{
    Handler2Responder* self = GG_AllocateZeroMemory(sizeof(Handler2Responder));
    GG_ASSERT(self);

    // create a timer
    GG_TimerScheduler_CreateTimer(GG_Loop_GetTimerScheduler(g_loop), &self->timer);
    GG_ASSERT(self->timer);

    // init members
    self->endpoint  = endpoint;
    self->handler   = handler;
    self->request   = request;
    self->responder = responder;
    self->block     = block;

    // setup the interface
    GG_SET_INTERFACE(self, Handler2Responder, GG_TimerListener);

    return self;
}

static GG_Result
Handler2_OnRequest(GG_CoapRequestHandler*   _self,
                   GG_CoapEndpoint*         endpoint,
                   const GG_CoapMessage*    request,
                   GG_CoapResponder*        responder,
                   const GG_BufferMetadata* transport_metadata,
                   GG_CoapMessage**         response)
{
    Handler2* self = GG_SELF(Handler2, GG_CoapRequestHandler);
    GG_COMPILER_UNUSED(responder);
    GG_COMPILER_UNUSED(transport_metadata);

    GG_CoapMessageBlockInfo block_info;
    GG_Result result = GG_CoapMessage_GetBlockInfo(request, GG_COAP_MESSAGE_OPTION_BLOCK2, &block_info, 1024);
    if (GG_FAILED(result)) {
        return GG_COAP_MESSAGE_CODE_BAD_OPTION;
    }

    if (self->response_delay) {
        // we're in async mode, create a responder to respond later
        GG_ASSERT(responder);
        Handler2Responder* async_responder = Handler2Responder_Create(self, endpoint, request, responder, block_info);
        if (async_responder) {
            GG_Timer_Schedule(async_responder->timer,
                              GG_CAST(async_responder, GG_TimerListener),
                              self->response_delay);
        }

        return GG_ERROR_WOULD_BLOCK;
    } else {
        // we're in sync mode, respond now
        *response = Handler2_CreateResponse(self, block_info, endpoint, request);
        return *response ? GG_SUCCESS : GG_ERROR_INTERNAL;
    }
}

GG_IMPLEMENT_INTERFACE(Handler2, GG_CoapRequestHandler) {
    Handler2_OnRequest
};

//----------------------------------------------------------------------
// CoAP handler that returns a large payload from a block source
//----------------------------------------------------------------------
typedef struct {
    GG_IMPLEMENTS(GG_CoapRequestHandler);

    GG_CoapBlockSource* block_source;
} Handler3;

static GG_Result
Handler3_OnRequest(GG_CoapRequestHandler*   _self,
                   GG_CoapEndpoint*         endpoint,
                   const GG_CoapMessage*    request,
                   GG_CoapResponder*        responder,
                   const GG_BufferMetadata* transport_metadata,
                   GG_CoapMessage**         response)
{
    Handler3* self = GG_SELF(Handler3, GG_CoapRequestHandler);
    GG_COMPILER_UNUSED(responder);
    GG_COMPILER_UNUSED(transport_metadata);

    GG_CoapMessageBlockInfo block_info;
    GG_Result result = GG_CoapMessage_GetBlockInfo(request, GG_COAP_MESSAGE_OPTION_BLOCK2, &block_info, 1024);
    if (GG_FAILED(result)) {
        return GG_COAP_MESSAGE_CODE_BAD_OPTION;
    }

    return GG_CoapEndpoint_CreateBlockwiseResponseFromBlockSource(endpoint,
                                                                  request,
                                                                  GG_COAP_MESSAGE_CODE_CONTENT,
                                                                  NULL, 0,
                                                                  self->block_source,
                                                                  GG_COAP_MESSAGE_OPTION_BLOCK2,
                                                                  &block_info,
                                                                  response);
}

GG_IMPLEMENT_INTERFACE(Handler3, GG_CoapRequestHandler) {
    Handler3_OnRequest
};

typedef enum {
    MODE_CLIENT_POST,
    MODE_CLIENT_GET,
    MODE_SERVER
} Mode;

//----------------------------------------------------------------------
// CoAP blockwise listener
//----------------------------------------------------------------------
typedef struct {
    GG_IMPLEMENTS(GG_CoapBlockwiseResponseListener);
} BlockListener;

static void
BlockListener_OnResponseBlock(GG_CoapBlockwiseResponseListener* _self,
                              GG_CoapMessageBlockInfo*          block_info,
                              GG_CoapMessage*                   block_message)
{
    GG_COMPILER_UNUSED(_self);

    printf("=== received block offset=%u, payload_size=%u, more=%s\n",
           (int)block_info->offset,
           (int)GG_CoapMessage_GetPayloadSize(block_message),
           block_info->more ? "true" : "false");

    if (!block_info->more) {
        printf("=== last block received, request done\n");
        GG_Loop_RequestTermination(g_loop);
    }
}

static void
BlockListener_OnError(GG_CoapBlockwiseResponseListener* _self,
                      GG_Result                         error,
                      const char*                       message)
{
    GG_COMPILER_UNUSED(_self);

    printf("!!! BlockListener error: %d %s\n", error, message ? message : "");

    GG_Loop_RequestTermination(g_loop);
}

GG_IMPLEMENT_INTERFACE(BlockListener, GG_CoapBlockwiseResponseListener) {
    .OnResponseBlock = BlockListener_OnResponseBlock,
    .OnError         = BlockListener_OnError
};

//----------------------------------------------------------------------
// Main entry point
//----------------------------------------------------------------------
int
main(int argc, char** argv)
{
    // parse command-line arguments
    if (argc < 2) {
        fprintf(stderr,
                "usage: gg-coap-blockwise-example client-post [<block-size>]|client-get [<block-size>]|server\n");
        return 1;
    }
    Mode mode;
    size_t preferred_block_size = 0;
    if (!strcmp(argv[1], "client-post")) {
        mode = MODE_CLIENT_POST;
        if (argc >= 3) {
            preferred_block_size = strtoul(argv[2], NULL, 10);
        }
    } else if (!strcmp(argv[1], "client-get")) {
        mode = MODE_CLIENT_GET;
        if (argc >= 3) {
            preferred_block_size = strtoul(argv[2], NULL, 10);
        }
    } else if (!strcmp(argv[1], "server")) {
        mode = MODE_SERVER;
    } else {
        fprintf(stderr, "ERROR: unexpected argument\n");
        return 1;
    }

    // let's announce ourselves
    printf("=== Golden Gate CoAP Blockwise Example ===\n");

    // setup the loop
    GG_Loop_Create(&g_loop);
    GG_Loop_BindToCurrentThread(g_loop);
    GG_Timestamp now = GG_System_GetCurrentTimestamp();
    uint32_t scheduler_time = (uint32_t)(now/GG_NANOSECONDS_PER_MILLISECOND);
    GG_TimerScheduler_SetTime(GG_Loop_GetTimerScheduler(g_loop), scheduler_time);

    // create a BSD socket to use as a transport
    GG_DatagramSocket* transport_socket = NULL;
    GG_SocketAddress transport_local_address  = { GG_IpAddress_Any, 5683 };
    GG_SocketAddress transport_remote_address = { GG_IpAddress_Any, 5683 };
    GG_IpAddress_SetFromString(&transport_remote_address.address, "127.0.0.1");
    GG_Result result = GG_BsdDatagramSocket_Create(mode == MODE_SERVER ? &transport_local_address : NULL,
                                                   &transport_remote_address,
                                                   false,
                                                   1280,
                                                   &transport_socket);
    if (GG_FAILED(result)) {
        fprintf(stderr, "ERROR: GG_BsdDatagramSocket_Create failed (%d)\n", result);
        return 1;
    }
    GG_DatagramSocket_Attach(transport_socket, g_loop);

    // create a CoAP endpoint
    GG_CoapEndpoint* endpoint = NULL;
    result = GG_CoapEndpoint_Create(GG_Loop_GetTimerScheduler(g_loop),
                                    GG_DatagramSocket_AsDataSink(transport_socket),
                                    GG_DatagramSocket_AsDataSource(transport_socket),
                                    &endpoint);

    // create a block source
    BlockSource block_source;
    GG_SET_INTERFACE(&block_source, BlockSource, GG_CoapBlockSource);
    block_source.payload_size = 1500;

    // create and attach a 'Handler1' CoAP handler
    Handler1 handler1;
    Handler1_Init(&handler1);
    GG_CoapEndpoint_RegisterRequestHandler(endpoint,
                                           "handler1",
                                           GG_COAP_REQUEST_HANDLER_FLAG_ALLOW_PUT |
                                           GG_COAP_REQUEST_HANDLER_FLAG_ALLOW_POST,
                                           GG_CAST(&handler1, GG_CoapRequestHandler));

    // create and attach a synchronous 'Handler2' CoAP handler
    Handler2 handler2 = {
        GG_INTERFACE_INITIALIZER(Handler2, GG_CoapRequestHandler)
    };
    handler2.payload_size   = 10000;
    handler2.response_delay = 0;
    GG_CoapEndpoint_RegisterRequestHandler(endpoint,
                                           "handler2",
                                           GG_COAP_REQUEST_HANDLER_FLAG_ALLOW_GET,
                                           GG_CAST(&handler2, GG_CoapRequestHandler));

    // create and attach an asynchronous 'Handler2' CoAP handler
    Handler2 handler2_async = {
        GG_INTERFACE_INITIALIZER(Handler2, GG_CoapRequestHandler)
    };
    handler2_async.payload_size   = 10000;
    handler2_async.response_delay = 1000;
    GG_CoapEndpoint_RegisterRequestHandler(endpoint,
                                           "handler2-async",
                                           GG_COAP_REQUEST_HANDLER_FLAG_ALLOW_GET |
                                           GG_COAP_REQUEST_HANDLER_FLAG_ENABLE_ASYNC,
                                           GG_CAST(&handler2_async, GG_CoapRequestHandler));

    // create and attach a synchronous 'Handler3' CoAP handler
    BlockSource block_source3;
    GG_SET_INTERFACE(&block_source3, BlockSource, GG_CoapBlockSource);
    block_source3.payload_size = 10000;
    Handler3 handler3;
    GG_SET_INTERFACE(&handler3, Handler3, GG_CoapRequestHandler);
    handler3.block_source = GG_CAST(&block_source3, GG_CoapBlockSource);
    GG_CoapEndpoint_RegisterRequestHandler(endpoint,
                                           "handler3",
                                           GG_COAP_REQUEST_HANDLER_FLAG_ALLOW_GET,
                                           GG_CAST(&handler3, GG_CoapRequestHandler));

    // create a blockwise listener
    BlockListener block_listener;
    GG_SET_INTERFACE(&block_listener, BlockListener, GG_CoapBlockwiseResponseListener);

    // if this is a client, send a request
    GG_CoapRequestHandle request_handle = 0;
    if (mode == MODE_CLIENT_POST) {
        GG_CoapMessageOptionParam params[2] = {
            GG_COAP_MESSAGE_OPTION_PARAM_STRING(URI_PATH, "large-post"),
            GG_COAP_MESSAGE_OPTION_PARAM_UINT(CONTENT_FORMAT, GG_COAP_MESSAGE_FORMAT_ID_TEXT_PLAIN)
        };
        result = GG_CoapEndpoint_SendBlockwiseRequest(endpoint,
                                                      GG_COAP_METHOD_POST,
                                                      params, GG_ARRAY_SIZE(params),
                                                      GG_CAST(&block_source, GG_CoapBlockSource),
                                                      preferred_block_size,
                                                      NULL,
                                                      GG_CAST(&block_listener, GG_CoapBlockwiseResponseListener),
                                                      &request_handle);
        if (GG_FAILED(result)) {
            fprintf(stderr, "ERROR: GG_CoapEndpoint_SendBlockwiseRequest failed (%d)\n", result);
            return 1;
        }
    } else if (mode == MODE_CLIENT_GET) {
        GG_CoapMessageOptionParam params[1] = {
            GG_COAP_MESSAGE_OPTION_PARAM_STRING(URI_PATH, "large")
        };
        result = GG_CoapEndpoint_SendBlockwiseRequest(endpoint,
                                                      GG_COAP_METHOD_GET,
                                                      params, GG_ARRAY_SIZE(params),
                                                      NULL,
                                                      preferred_block_size,
                                                      NULL,
                                                      GG_CAST(&block_listener, GG_CoapBlockwiseResponseListener),
                                                      &request_handle);
        if (GG_FAILED(result)) {
            fprintf(stderr, "ERROR: GG_CoapEndpoint_SendBlockwiseRequest failed (%d)\n", result);
            return 1;
        }
    }

    // run the loop
    GG_Loop_Run(g_loop);

    // cleanup
    GG_Loop_Destroy(g_loop);
    return 0;
}

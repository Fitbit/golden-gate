// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

#include "CppUTest/TestHarness.h"
#include "CppUTest/MemoryLeakDetectorNewMacros.h"

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <string.h>

#include "xp/common/gg_io.h"
#include "xp/common/gg_port.h"
#include "xp/common/gg_buffer.h"
#include "xp/common/gg_memory.h"
#include "xp/coap/gg_coap.h"
#include "xp/coap/gg_coap_message.h"
#include "xp/coap/gg_coap_blockwise.h"
#include "xp/utils/gg_async_pipe.h"

/*----------------------------------------------------------------------
|   tests
+---------------------------------------------------------------------*/
TEST_GROUP(GG_COAP_BLOCKWISE) {
    void setup(void) {
    }

    void teardown(void) {
    }
};

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

    size_t                  blocks_received;
    size_t                  bytes_received;
    GG_CoapMessageBlockInfo last_block_info;
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

    GG_CoapMessageBlockInfo block_info;
    GG_Result result = GG_CoapMessage_GetBlockInfo(request, GG_COAP_MESSAGE_OPTION_BLOCK1, &block_info, 0);
    if (GG_FAILED(result)) {
        return GG_COAP_MESSAGE_CODE_BAD_OPTION;
    }
    self->last_block_info = block_info;
    ++self->blocks_received;
    self->bytes_received += GG_CoapMessage_GetPayloadSize(request);

    return GG_CoapEndpoint_CreateBlockwiseResponse(endpoint,
                                                   request,
                                                   block_info.more ?
                                                   GG_COAP_MESSAGE_CODE_CONTINUE :
                                                   GG_COAP_MESSAGE_CODE_CHANGED,
                                                   NULL, 0,
                                                   NULL, 0,
                                                   GG_COAP_MESSAGE_OPTION_BLOCK1,
                                                   &block_info,
                                                   response);
}

GG_IMPLEMENT_INTERFACE(Handler1, GG_CoapRequestHandler) {
    Handler1_OnRequest
};

//----------------------------------------------------------------------
// CoAP handler that returns a large payload, possibly asynchronously
//----------------------------------------------------------------------
typedef struct {
    GG_IMPLEMENTS(GG_CoapRequestHandler);

    size_t             payload_size;
    unsigned int       response_delay;
    GG_TimerScheduler* scheduler;
} Handler2;

typedef struct {
    GG_IMPLEMENTS(GG_TimerListener);

    GG_TimerScheduler*      scheduler;
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
    size_t chunk_size = block_info.size;
    GG_CoapMessageBlockInfo_AdjustAndGetChunkSize(block_info.offset,
                                                  &chunk_size,
                                                  &block_info.more,
                                                  self->payload_size);
    uint8_t payload[1024];
    for (unsigned int i = 0; i < chunk_size; i++) {
        payload[i] = (uint8_t)('A' + (block_info.offset / block_info.size));
    }

    GG_CoapMessage* response = NULL;
    GG_Result result = GG_CoapEndpoint_CreateBlockwiseResponse(endpoint,
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
    Handler2Responder* self = (Handler2Responder*)GG_AllocateZeroMemory(sizeof(Handler2Responder));

    // create a timer
    GG_TimerScheduler_CreateTimer(self->scheduler, &self->timer);

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

//----------------------------------------------------------------------
// CoAP handler that returns a predefined sequence of responses
//----------------------------------------------------------------------
typedef struct {
    size_t                  payload_size;
    uint8_t                 response_code;
    unsigned int            option;
    GG_CoapMessageBlockInfo block_info;
} Handler4Item;

typedef struct {
    GG_IMPLEMENTS(GG_CoapRequestHandler);

    Handler4Item*           items;
    size_t                  item_count;
    size_t                  current_item;
    size_t                  blocks_received;
    size_t                  bytes_received;
    size_t                  blocks_sent;
    size_t                  bytes_sent;
    GG_CoapMessageBlockInfo* received_block1_options;
    GG_CoapMessageBlockInfo* received_block2_options;
} Handler4;

static GG_Result
Handler4_OnRequest(GG_CoapRequestHandler*   _self,
                   GG_CoapEndpoint*         endpoint,
                   const GG_CoapMessage*    request,
                   GG_CoapResponder*        responder,
                   const GG_BufferMetadata* transport_metadata,
                   GG_CoapMessage**         response)
{
    Handler4* self = GG_SELF(Handler4, GG_CoapRequestHandler);
    GG_COMPILER_UNUSED(responder);
    GG_COMPILER_UNUSED(transport_metadata);

    if (self->current_item >= self->item_count) {
        return GG_FAILURE;
    }

    Handler4Item* item = &self->items[self->current_item];

    if (self->received_block1_options) {
        GG_CoapMessage_GetBlockInfo(request,
                                    GG_COAP_MESSAGE_OPTION_BLOCK1,
                                    &self->received_block1_options[self->current_item], 0);
    }
    if (self->received_block2_options) {
        GG_CoapMessage_GetBlockInfo(request,
                                    GG_COAP_MESSAGE_OPTION_BLOCK2,
                                    &self->received_block2_options[self->current_item], 0);
    }

    // keep track of blocks and bytes received
    ++self->blocks_received;
    self->bytes_received += GG_CoapMessage_GetPayloadSize(request);

    // prepare the payload to return
    uint8_t payload[1024];
    size_t  payload_size = item->payload_size;
    if (payload_size) {
        memset(payload, (uint8_t)self->current_item, payload_size);
    }

    // keep track of how many blocks and bytes we've sent
    ++self->blocks_sent;
    self->bytes_sent += payload_size;

    // increment the item counter
    ++self->current_item;

    // send the response
    if (item->option) {
        return GG_CoapEndpoint_CreateBlockwiseResponse(endpoint,
                                                       request,
                                                       item->response_code,
                                                       NULL, 0,
                                                       payload_size ? payload : NULL, payload_size,
                                                       item->option,
                                                       &item->block_info,
                                                       response);
    } else {
        return GG_CoapEndpoint_CreateResponse(endpoint,
                                              request,
                                              item->response_code,
                                              NULL, 0,
                                              payload_size ? payload : NULL, payload_size,
                                              response);
    }
}

GG_IMPLEMENT_INTERFACE(Handler4, GG_CoapRequestHandler) {
    Handler4_OnRequest
};

//----------------------------------------------------------------------
// CoAP blockwise listener
//----------------------------------------------------------------------
typedef struct {
    GG_IMPLEMENTS(GG_CoapBlockwiseResponseListener);

    size_t                  blocks_received;
    size_t                  bytes_received;
    GG_Result               last_error;
    GG_CoapMessageBlockInfo last_block_info;
    uint8_t                 last_code;
} BlockListener;

static void
BlockListener_OnResponseBlock(GG_CoapBlockwiseResponseListener* _self,
                              GG_CoapMessageBlockInfo*          block_info,
                              GG_CoapMessage*                   block_message)
{
    BlockListener* self = GG_SELF(BlockListener, GG_CoapBlockwiseResponseListener);

    self->last_code = GG_CoapMessage_GetCode(block_message);
    self->last_block_info = *block_info;
    if (GG_COAP_MESSAGE_CODE_CLASS(GG_CoapMessage_GetCode(block_message)) ==
        GG_COAP_MESSAGE_CODE_CLASS_SUCCESS_RESPONSE) {
        ++self->blocks_received;
        self->bytes_received += GG_CoapMessage_GetPayloadSize(block_message);
    }
}

static void
BlockListener_OnError(GG_CoapBlockwiseResponseListener* _self,
                      GG_Result                         error,
                      const char*                       message)
{
    BlockListener* self = GG_SELF(BlockListener, GG_CoapBlockwiseResponseListener);
    GG_COMPILER_UNUSED(message);

    self->last_error = error;
}

GG_IMPLEMENT_INTERFACE(BlockListener, GG_CoapBlockwiseResponseListener) {
    .OnResponseBlock = BlockListener_OnResponseBlock,
    .OnError         = BlockListener_OnError
};

//-----------------------------------------------------------------------
TEST(GG_COAP_BLOCKWISE, Test_BasicBlockwiseGet) {
    GG_Result result;

    // create two endpoints
    GG_TimerScheduler* timer_scheduler1 = NULL;
    GG_TimerScheduler_Create(&timer_scheduler1);
    GG_CoapEndpoint* endpoint1;
    GG_CoapEndpoint_Create(timer_scheduler1, NULL, NULL, &endpoint1);
    GG_TimerScheduler* timer_scheduler2 = NULL;
    GG_TimerScheduler_Create(&timer_scheduler2);
    GG_CoapEndpoint* endpoint2;
    GG_CoapEndpoint_Create(timer_scheduler2, NULL, NULL, &endpoint2);

    // connect the two endpoints with async pipes
    GG_AsyncPipe* pipe1 = NULL;
    result = GG_AsyncPipe_Create(timer_scheduler1, 1, &pipe1);
    CHECK_EQUAL(GG_SUCCESS, result);
    GG_AsyncPipe* pipe2 = NULL;
    result = GG_AsyncPipe_Create(timer_scheduler2, 1, &pipe2);
    CHECK_EQUAL(GG_SUCCESS, result);
    GG_DataSource_SetDataSink(GG_CoapEndpoint_AsDataSource(endpoint1),
                              GG_AsyncPipe_AsDataSink(pipe1));
    GG_DataSource_SetDataSink(GG_AsyncPipe_AsDataSource(pipe1),
                              GG_CoapEndpoint_AsDataSink(endpoint2));
    GG_DataSource_SetDataSink(GG_CoapEndpoint_AsDataSource(endpoint2),
                              GG_AsyncPipe_AsDataSink(pipe2));
    GG_DataSource_SetDataSink(GG_AsyncPipe_AsDataSource(pipe2),
                              GG_CoapEndpoint_AsDataSink(endpoint1));

    // create and register a handler2
    Handler2 handler2 = {
        GG_INTERFACE_INITIALIZER(Handler2, GG_CoapRequestHandler)
    };
    handler2.payload_size   = 10000;
    handler2.response_delay = 0;
    GG_CoapEndpoint_RegisterRequestHandler(endpoint2,
                                           "handler2",
                                           GG_COAP_REQUEST_HANDLER_FLAG_ALLOW_GET,
                                           GG_CAST(&handler2, GG_CoapRequestHandler));

    // create a block source
    BlockSource block_source = {
        GG_INTERFACE_INITIALIZER(BlockSource, GG_CoapBlockSource)
    };
    block_source.payload_size = 10000;

    // create and register a handler3
    Handler3 handler3 = {
        GG_INTERFACE_INITIALIZER(Handler3, GG_CoapRequestHandler)
    };
    handler3.block_source = GG_CAST(&block_source, GG_CoapBlockSource);
    GG_CoapEndpoint_RegisterRequestHandler(endpoint2,
                                           "handler3",
                                           GG_COAP_REQUEST_HANDLER_FLAG_ALLOW_GET,
                                           GG_CAST(&handler3, GG_CoapRequestHandler));

    // create a blockwise listener
    BlockListener block_listener = {
        GG_INTERFACE_INITIALIZER(BlockListener, GG_CoapBlockwiseResponseListener)
    };

    // make a blockwise GET request for handler2
    GG_CoapRequestHandle request_handle;
    GG_CoapMessageOptionParam params1[1] = {
        GG_COAP_MESSAGE_OPTION_PARAM_STRING(URI_PATH, "handler2")
    };
    result = GG_CoapEndpoint_SendBlockwiseRequest(endpoint1,
                                                  GG_COAP_METHOD_GET,
                                                  params1, GG_ARRAY_SIZE(params1),
                                                  NULL,
                                                  0,
                                                  NULL,
                                                  GG_CAST(&block_listener, GG_CoapBlockwiseResponseListener),
                                                  &request_handle);
    LONGS_EQUAL(GG_SUCCESS, result);
    LONGS_EQUAL(0, block_listener.blocks_received);
    CHECK_FALSE(request_handle == GG_COAP_INVALID_REQUEST_HANDLE)

    uint32_t now1 = 0;
    GG_TimerScheduler_SetTime(timer_scheduler1, ++now1);
    LONGS_EQUAL(0, block_listener.blocks_received);

    uint32_t now2 = 0;
    GG_TimerScheduler_SetTime(timer_scheduler2, ++now2);
    LONGS_EQUAL(1, block_listener.blocks_received);
    LONGS_EQUAL(1024, block_listener.bytes_received);
    LONGS_EQUAL(0, block_listener.last_block_info.offset);
    LONGS_EQUAL(1024, block_listener.last_block_info.size);
    LONGS_EQUAL(1, block_listener.last_block_info.more);

    for (unsigned int i = 0; i < 100; i++) {
        GG_TimerScheduler_SetTime(timer_scheduler1, ++now1);
        GG_TimerScheduler_SetTime(timer_scheduler2, ++now2);
    }

    LONGS_EQUAL(10, block_listener.blocks_received);
    LONGS_EQUAL(10000, block_listener.bytes_received);
    LONGS_EQUAL(9 * 1024, block_listener.last_block_info.offset);
    LONGS_EQUAL(1024, block_listener.last_block_info.size);
    LONGS_EQUAL(0, block_listener.last_block_info.more);

    // reset some counters
    block_listener.blocks_received = 0;
    block_listener.bytes_received = 0;
    block_listener.last_error = GG_SUCCESS;
    block_listener.last_block_info.offset = 0;
    block_listener.last_block_info.size = 0;
    block_listener.last_block_info.more = false;
    GG_TimerScheduler_SetTime(timer_scheduler1, 0);
    GG_TimerScheduler_SetTime(timer_scheduler2, 0);

    // make a blockwise GET request for handler3
    GG_CoapMessageOptionParam params2[1] = {
        GG_COAP_MESSAGE_OPTION_PARAM_STRING(URI_PATH, "handler3")
    };
    result = GG_CoapEndpoint_SendBlockwiseRequest(endpoint1,
                                                  GG_COAP_METHOD_GET,
                                                  params2, GG_ARRAY_SIZE(params2),
                                                  NULL,
                                                  0,
                                                  NULL,
                                                  GG_CAST(&block_listener, GG_CoapBlockwiseResponseListener),
                                                  &request_handle);
    LONGS_EQUAL(GG_SUCCESS, result);
    LONGS_EQUAL(0, block_listener.blocks_received);

    now1 = 0;
    GG_TimerScheduler_SetTime(timer_scheduler1, ++now1);
    LONGS_EQUAL(0, block_listener.blocks_received);

    now2 = 0;
    GG_TimerScheduler_SetTime(timer_scheduler2, ++now2);
    LONGS_EQUAL(1, block_listener.blocks_received);
    LONGS_EQUAL(1024, block_listener.bytes_received);
    LONGS_EQUAL(0, block_listener.last_block_info.offset);
    LONGS_EQUAL(1024, block_listener.last_block_info.size);
    LONGS_EQUAL(1, block_listener.last_block_info.more);

    for (unsigned int i = 0; i < 100; i++) {
        GG_TimerScheduler_SetTime(timer_scheduler1, ++now1);
        GG_TimerScheduler_SetTime(timer_scheduler2, ++now2);
    }

    LONGS_EQUAL(10, block_listener.blocks_received);
    LONGS_EQUAL(10000, block_listener.bytes_received);
    LONGS_EQUAL(9 * 1024, block_listener.last_block_info.offset);
    LONGS_EQUAL(1024, block_listener.last_block_info.size);
    LONGS_EQUAL(0, block_listener.last_block_info.more);

    // cleanup
    GG_DataSource_SetDataSink(GG_CoapEndpoint_AsDataSource(endpoint1), NULL);
    GG_DataSource_SetDataSink(GG_AsyncPipe_AsDataSource(pipe1), NULL);
    GG_DataSource_SetDataSink(GG_CoapEndpoint_AsDataSource(endpoint2), NULL);
    GG_DataSource_SetDataSink(GG_AsyncPipe_AsDataSource(pipe2), NULL);
    GG_CoapEndpoint_Destroy(endpoint1);
    GG_CoapEndpoint_Destroy(endpoint2);
    GG_AsyncPipe_Destroy(pipe1);
    GG_AsyncPipe_Destroy(pipe2);
    GG_TimerScheduler_Destroy(timer_scheduler1);
    GG_TimerScheduler_Destroy(timer_scheduler2);
}

//-----------------------------------------------------------------------
TEST(GG_COAP_BLOCKWISE, Test_BasicBlockwisePut) {
    GG_Result result;

    // create two endpoints
    GG_TimerScheduler* timer_scheduler1 = NULL;
    GG_TimerScheduler_Create(&timer_scheduler1);
    GG_CoapEndpoint* endpoint1;
    GG_CoapEndpoint_Create(timer_scheduler1, NULL, NULL, &endpoint1);
    GG_TimerScheduler* timer_scheduler2 = NULL;
    GG_TimerScheduler_Create(&timer_scheduler2);
    GG_CoapEndpoint* endpoint2;
    GG_CoapEndpoint_Create(timer_scheduler2, NULL, NULL, &endpoint2);

    // connect the two endpoints with async pipes
    GG_AsyncPipe* pipe1 = NULL;
    result = GG_AsyncPipe_Create(timer_scheduler1, 1, &pipe1);
    CHECK_EQUAL(GG_SUCCESS, result);
    GG_AsyncPipe* pipe2 = NULL;
    result = GG_AsyncPipe_Create(timer_scheduler2, 1, &pipe2);
    CHECK_EQUAL(GG_SUCCESS, result);
    GG_DataSource_SetDataSink(GG_CoapEndpoint_AsDataSource(endpoint1),
                              GG_AsyncPipe_AsDataSink(pipe1));
    GG_DataSource_SetDataSink(GG_AsyncPipe_AsDataSource(pipe1),
                              GG_CoapEndpoint_AsDataSink(endpoint2));
    GG_DataSource_SetDataSink(GG_CoapEndpoint_AsDataSource(endpoint2),
                              GG_AsyncPipe_AsDataSink(pipe2));
    GG_DataSource_SetDataSink(GG_AsyncPipe_AsDataSource(pipe2),
                              GG_CoapEndpoint_AsDataSink(endpoint1));

    // create a handler
    Handler1 handler1 = {
        GG_INTERFACE_INITIALIZER(Handler1, GG_CoapRequestHandler)
    };
    GG_CoapEndpoint_RegisterRequestHandler(endpoint2,
                                           "handler1",
                                           GG_COAP_REQUEST_HANDLER_FLAG_ALLOW_PUT,
                                           GG_CAST(&handler1, GG_CoapRequestHandler));

    // create a blockwise listener
    BlockListener block_listener = {
        GG_INTERFACE_INITIALIZER(BlockListener, GG_CoapBlockwiseResponseListener)
    };

    // create a block source
    BlockSource block_source = {
        GG_INTERFACE_INITIALIZER(BlockSource, GG_CoapBlockSource)
    };
    block_source.payload_size = 10000;

    // make a blockwise PUT request
    GG_CoapRequestHandle request_handle;
    GG_CoapMessageOptionParam params[1] = {
        GG_COAP_MESSAGE_OPTION_PARAM_STRING(URI_PATH, "handler1")
    };
    result = GG_CoapEndpoint_SendBlockwiseRequest(endpoint1,
                                                  GG_COAP_METHOD_PUT,
                                                  params, GG_ARRAY_SIZE(params),
                                                  GG_CAST(&block_source, GG_CoapBlockSource),
                                                  0,
                                                  NULL,
                                                  GG_CAST(&block_listener, GG_CoapBlockwiseResponseListener),
                                                  &request_handle);
    LONGS_EQUAL(GG_SUCCESS, result);
    LONGS_EQUAL(0, handler1.blocks_received);
    LONGS_EQUAL(0, handler1.bytes_received);

    uint32_t now1 = 0;
    GG_TimerScheduler_SetTime(timer_scheduler1, ++now1);
    LONGS_EQUAL(1, handler1.blocks_received);
    LONGS_EQUAL(1024, handler1.bytes_received);

    uint32_t now2 = 0;
    GG_TimerScheduler_SetTime(timer_scheduler2, ++now2);
    LONGS_EQUAL(1, handler1.blocks_received);
    LONGS_EQUAL(1024, handler1.bytes_received);
    LONGS_EQUAL(0, handler1.last_block_info.offset);
    LONGS_EQUAL(1024, handler1.last_block_info.size);
    LONGS_EQUAL(1, handler1.last_block_info.more);

    for (unsigned int i = 0; i < 100; i++) {
        GG_TimerScheduler_SetTime(timer_scheduler1, ++now1);
        GG_TimerScheduler_SetTime(timer_scheduler2, ++now2);
    }

    LONGS_EQUAL(10, handler1.blocks_received);
    LONGS_EQUAL(10000, handler1.bytes_received);
    LONGS_EQUAL(9 * 1024, handler1.last_block_info.offset);
    LONGS_EQUAL(1024, handler1.last_block_info.size);
    LONGS_EQUAL(0, handler1.last_block_info.more);

    // cleanup
    GG_DataSource_SetDataSink(GG_CoapEndpoint_AsDataSource(endpoint1), NULL);
    GG_DataSource_SetDataSink(GG_AsyncPipe_AsDataSource(pipe1), NULL);
    GG_DataSource_SetDataSink(GG_CoapEndpoint_AsDataSource(endpoint2), NULL);
    GG_DataSource_SetDataSink(GG_AsyncPipe_AsDataSource(pipe2), NULL);
    GG_CoapEndpoint_Destroy(endpoint1);
    GG_CoapEndpoint_Destroy(endpoint2);
    GG_AsyncPipe_Destroy(pipe1);
    GG_AsyncPipe_Destroy(pipe2);
    GG_TimerScheduler_Destroy(timer_scheduler1);
    GG_TimerScheduler_Destroy(timer_scheduler2);
}

//-----------------------------------------------------------------------
TEST(GG_COAP_BLOCKWISE, Test_BasicBlockwiseCancel) {
    GG_Result result;

    // create two endpoints
    GG_TimerScheduler* timer_scheduler1 = NULL;
    GG_TimerScheduler_Create(&timer_scheduler1);
    GG_CoapEndpoint* endpoint1;
    GG_CoapEndpoint_Create(timer_scheduler1, NULL, NULL, &endpoint1);
    GG_TimerScheduler* timer_scheduler2 = NULL;
    GG_TimerScheduler_Create(&timer_scheduler2);
    GG_CoapEndpoint* endpoint2;
    GG_CoapEndpoint_Create(timer_scheduler2, NULL, NULL, &endpoint2);

    // connect the two endpoints with async pipes
    GG_AsyncPipe* pipe1 = NULL;
    result = GG_AsyncPipe_Create(timer_scheduler1, 1, &pipe1);
    CHECK_EQUAL(GG_SUCCESS, result);
    GG_AsyncPipe* pipe2 = NULL;
    result = GG_AsyncPipe_Create(timer_scheduler2, 1, &pipe2);
    CHECK_EQUAL(GG_SUCCESS, result);
    GG_DataSource_SetDataSink(GG_CoapEndpoint_AsDataSource(endpoint1),
                              GG_AsyncPipe_AsDataSink(pipe1));
    GG_DataSource_SetDataSink(GG_AsyncPipe_AsDataSource(pipe1),
                              GG_CoapEndpoint_AsDataSink(endpoint2));
    GG_DataSource_SetDataSink(GG_CoapEndpoint_AsDataSource(endpoint2),
                              GG_AsyncPipe_AsDataSink(pipe2));
    GG_DataSource_SetDataSink(GG_AsyncPipe_AsDataSource(pipe2),
                              GG_CoapEndpoint_AsDataSink(endpoint1));

    // create and register a handler2
    Handler2 handler2 = {
        GG_INTERFACE_INITIALIZER(Handler2, GG_CoapRequestHandler)
    };
    handler2.payload_size   = 10000;
    handler2.response_delay = 0;
    GG_CoapEndpoint_RegisterRequestHandler(endpoint2,
                                           "handler2",
                                           GG_COAP_REQUEST_HANDLER_FLAG_ALLOW_GET,
                                           GG_CAST(&handler2, GG_CoapRequestHandler));

    // create a blockwise listener
    BlockListener block_listener = {
        GG_INTERFACE_INITIALIZER(BlockListener, GG_CoapBlockwiseResponseListener)
    };

    // make a blockwise GET request for handler2
    GG_CoapRequestHandle request_handle;
    GG_CoapMessageOptionParam params[1] = {
        GG_COAP_MESSAGE_OPTION_PARAM_STRING(URI_PATH, "handler2")
    };
    result = GG_CoapEndpoint_SendBlockwiseRequest(endpoint1,
                                                  GG_COAP_METHOD_GET,
                                                  params, GG_ARRAY_SIZE(params),
                                                  NULL,
                                                  0,
                                                  NULL,
                                                  GG_CAST(&block_listener, GG_CoapBlockwiseResponseListener),
                                                  &request_handle);
    LONGS_EQUAL(GG_SUCCESS, result);

    // cancel the request
    result = GG_CoapEndpoint_CancelBlockwiseRequest(endpoint1, request_handle);
    LONGS_EQUAL(GG_SUCCESS, result);

    // check that we can't cancel the same request twice
    result = GG_CoapEndpoint_CancelBlockwiseRequest(endpoint1, request_handle);
    LONGS_EQUAL(GG_ERROR_NO_SUCH_ITEM, result);

    // advance the protocol a bit
    uint32_t now1 = 0;
    uint32_t now2 = 0;
    for (unsigned int i = 0; i < 5; i++) {
        GG_TimerScheduler_SetTime(timer_scheduler1, ++now1);
        GG_TimerScheduler_SetTime(timer_scheduler2, ++now2);
    }

    // resend the request
    result = GG_CoapEndpoint_SendBlockwiseRequest(endpoint1,
                                                  GG_COAP_METHOD_GET,
                                                  params, GG_ARRAY_SIZE(params),
                                                  NULL,
                                                  0,
                                                  NULL,
                                                  GG_CAST(&block_listener, GG_CoapBlockwiseResponseListener),
                                                  &request_handle);
    LONGS_EQUAL(GG_SUCCESS, result);

    // advance the protocol a bit
    for (unsigned int i = 0; i < 5; i++) {
        GG_TimerScheduler_SetTime(timer_scheduler1, ++now1);
        GG_TimerScheduler_SetTime(timer_scheduler2, ++now2);
    }
    LONGS_EQUAL(5, block_listener.blocks_received);

    // cancel the request
    result = GG_CoapEndpoint_CancelBlockwiseRequest(endpoint1, request_handle);
    LONGS_EQUAL(GG_SUCCESS, result);

    // advance the protocol
    for (unsigned int i = 0; i < 10; i++) {
        GG_TimerScheduler_SetTime(timer_scheduler1, ++now1);
        GG_TimerScheduler_SetTime(timer_scheduler2, ++now2);
    }

    // check that nothing has changed
    LONGS_EQUAL(5, block_listener.blocks_received);

    // cleanup
    GG_DataSource_SetDataSink(GG_CoapEndpoint_AsDataSource(endpoint1), NULL);
    GG_DataSource_SetDataSink(GG_AsyncPipe_AsDataSource(pipe1), NULL);
    GG_DataSource_SetDataSink(GG_CoapEndpoint_AsDataSource(endpoint2), NULL);
    GG_DataSource_SetDataSink(GG_AsyncPipe_AsDataSource(pipe2), NULL);
    GG_CoapEndpoint_Destroy(endpoint1);
    GG_CoapEndpoint_Destroy(endpoint2);
    GG_AsyncPipe_Destroy(pipe1);
    GG_AsyncPipe_Destroy(pipe2);
    GG_TimerScheduler_Destroy(timer_scheduler1);
    GG_TimerScheduler_Destroy(timer_scheduler2);
}

//-----------------------------------------------------------------------
TEST(GG_COAP_BLOCKWISE, Test_BasicBlockwiseTimeout) {
    GG_Result result;

    // create one endpoint
    GG_TimerScheduler* timer_scheduler1 = NULL;
    GG_TimerScheduler_Create(&timer_scheduler1);
    GG_CoapEndpoint* endpoint1;
    GG_CoapEndpoint_Create(timer_scheduler1, NULL, NULL, &endpoint1);

    // connect the endpoint to an async pipe
    GG_AsyncPipe* pipe1 = NULL;
    result = GG_AsyncPipe_Create(timer_scheduler1, 1, &pipe1);
    CHECK_EQUAL(GG_SUCCESS, result);
    GG_DataSource_SetDataSink(GG_CoapEndpoint_AsDataSource(endpoint1),
                              GG_AsyncPipe_AsDataSink(pipe1));

    // create a blockwise listener
    BlockListener block_listener = {
        GG_INTERFACE_INITIALIZER(BlockListener, GG_CoapBlockwiseResponseListener)
    };

    // make a blockwise GET request for handler2
    GG_CoapRequestHandle request_handle;
    GG_CoapMessageOptionParam params1[1] = {
        GG_COAP_MESSAGE_OPTION_PARAM_STRING(URI_PATH, "timeout")
    };
    result = GG_CoapEndpoint_SendBlockwiseRequest(endpoint1,
                                                  GG_COAP_METHOD_GET,
                                                  params1, GG_ARRAY_SIZE(params1),
                                                  NULL,
                                                  0,
                                                  NULL,
                                                  GG_CAST(&block_listener, GG_CoapBlockwiseResponseListener),
                                                  &request_handle);
    LONGS_EQUAL(GG_SUCCESS, result);
    LONGS_EQUAL(0, block_listener.blocks_received);

    for (uint32_t now = 0; now < 500000; now += 1000) {
        GG_TimerScheduler_SetTime(timer_scheduler1, now);
    }

    LONGS_EQUAL(0, block_listener.blocks_received);
    LONGS_EQUAL(0, block_listener.bytes_received);
    LONGS_EQUAL(GG_ERROR_TIMEOUT, block_listener.last_error);

    // cleanup
    GG_CoapEndpoint_Destroy(endpoint1);
    GG_AsyncPipe_Destroy(pipe1);
    GG_TimerScheduler_Destroy(timer_scheduler1);
}

static void
Handler4_TestRun(Handler4*                         handler,
                 uint32_t                          handler_flags,
                 GG_CoapBlockSource*               source,
                 GG_CoapBlockwiseResponseListener* listener,
                 GG_CoapMethod                     method,
                 size_t                            preferred_block_size)
{
    GG_Result result;

    // create two endpoints
    GG_TimerScheduler* timer_scheduler1 = NULL;
    GG_TimerScheduler_Create(&timer_scheduler1);
    GG_CoapEndpoint* endpoint1;
    GG_CoapEndpoint_Create(timer_scheduler1, NULL, NULL, &endpoint1);
    GG_TimerScheduler* timer_scheduler2 = NULL;
    GG_TimerScheduler_Create(&timer_scheduler2);
    GG_CoapEndpoint* endpoint2;
    GG_CoapEndpoint_Create(timer_scheduler2, NULL, NULL, &endpoint2);

    // connect the two endpoints with async pipes
    GG_AsyncPipe* pipe1 = NULL;
    result = GG_AsyncPipe_Create(timer_scheduler1, 1, &pipe1);
    CHECK_EQUAL(GG_SUCCESS, result);
    GG_AsyncPipe* pipe2 = NULL;
    result = GG_AsyncPipe_Create(timer_scheduler2, 1, &pipe2);
    CHECK_EQUAL(GG_SUCCESS, result);
    GG_DataSource_SetDataSink(GG_CoapEndpoint_AsDataSource(endpoint1),
                              GG_AsyncPipe_AsDataSink(pipe1));
    GG_DataSource_SetDataSink(GG_AsyncPipe_AsDataSource(pipe1),
                              GG_CoapEndpoint_AsDataSink(endpoint2));
    GG_DataSource_SetDataSink(GG_CoapEndpoint_AsDataSource(endpoint2),
                              GG_AsyncPipe_AsDataSink(pipe2));
    GG_DataSource_SetDataSink(GG_AsyncPipe_AsDataSource(pipe2),
                              GG_CoapEndpoint_AsDataSink(endpoint1));

    // register the handler
    GG_CoapEndpoint_RegisterRequestHandler(endpoint2,
                                           "handler4",
                                           handler_flags,
                                           GG_CAST(handler, GG_CoapRequestHandler));

    // make a request
    GG_CoapRequestHandle request_handle;
    GG_CoapMessageOptionParam params[1] = {
        GG_COAP_MESSAGE_OPTION_PARAM_STRING(URI_PATH, "handler4")
    };
    result = GG_CoapEndpoint_SendBlockwiseRequest(endpoint1,
                                                  method,
                                                  params, GG_ARRAY_SIZE(params),
                                                  source,
                                                  preferred_block_size,
                                                  NULL,
                                                  listener,
                                                  &request_handle);
    LONGS_EQUAL(GG_SUCCESS, result);

    // advance the protocol
    uint32_t now1 = 0;
    uint32_t now2 = 0;
    for (unsigned int i = 0; i < 100; i++) {
        GG_TimerScheduler_SetTime(timer_scheduler1, ++now1);
        GG_TimerScheduler_SetTime(timer_scheduler2, ++now2);
    }

    // cleanup
    GG_DataSource_SetDataSink(GG_CoapEndpoint_AsDataSource(endpoint1), NULL);
    GG_DataSource_SetDataSink(GG_AsyncPipe_AsDataSource(pipe1), NULL);
    GG_DataSource_SetDataSink(GG_CoapEndpoint_AsDataSource(endpoint2), NULL);
    GG_DataSource_SetDataSink(GG_AsyncPipe_AsDataSource(pipe2), NULL);
    GG_CoapEndpoint_Destroy(endpoint1);
    GG_CoapEndpoint_Destroy(endpoint2);
    GG_AsyncPipe_Destroy(pipe1);
    GG_AsyncPipe_Destroy(pipe2);
    GG_TimerScheduler_Destroy(timer_scheduler1);
    GG_TimerScheduler_Destroy(timer_scheduler2);
}

//-----------------------------------------------------------------------
// Test that a blockwise client can receive a response from a non-blockwise-aware server
//-----------------------------------------------------------------------
TEST(GG_COAP_BLOCKWISE, Test_SimpleNonBlockwiseResponse) {
    BlockListener block_listener = {
        GG_INTERFACE_INITIALIZER(BlockListener, GG_CoapBlockwiseResponseListener)
    };

    Handler4 handler = {
        GG_INTERFACE_INITIALIZER(Handler4, GG_CoapRequestHandler)
    };
    Handler4Item items[] = {
        {
            .payload_size = 123,
            .response_code = GG_COAP_MESSAGE_CODE_CONTENT,
            .option        = 0
        }
    };
    handler.items = items;
    handler.item_count = GG_ARRAY_SIZE(items);

    Handler4_TestRun(&handler,
                     GG_COAP_REQUEST_HANDLER_FLAG_ALLOW_GET,
                     NULL,
                     GG_CAST(&block_listener, GG_CoapBlockwiseResponseListener),
                     GG_COAP_METHOD_GET,
                     0);

    LONGS_EQUAL(GG_SUCCESS, block_listener.last_error);
    LONGS_EQUAL(1, block_listener.blocks_received);
    LONGS_EQUAL(123, block_listener.bytes_received);
    LONGS_EQUAL(0, block_listener.last_block_info.more);
}

//-----------------------------------------------------------------------
// Test a blockwise GET request to a handler that returns a non-success code
//-----------------------------------------------------------------------
TEST(GG_COAP_BLOCKWISE, Test_SimpleGetWithError) {
    BlockListener block_listener = {
        GG_INTERFACE_INITIALIZER(BlockListener, GG_CoapBlockwiseResponseListener)
    };

    Handler4 handler = {
        GG_INTERFACE_INITIALIZER(Handler4, GG_CoapRequestHandler)
    };
    Handler4Item items[] = {
        {
            .payload_size  = 0,
            .response_code = GG_COAP_MESSAGE_CODE_FORBIDDEN,
            .option        = 0
        }
    };
    handler.items = items;
    handler.item_count = GG_ARRAY_SIZE(items);

    Handler4_TestRun(&handler,
                     GG_COAP_REQUEST_HANDLER_FLAG_ALLOW_GET,
                     NULL,
                     GG_CAST(&block_listener, GG_CoapBlockwiseResponseListener),
                     GG_COAP_METHOD_GET,
                     0);

    LONGS_EQUAL(GG_SUCCESS, block_listener.last_error);
    LONGS_EQUAL(0, block_listener.blocks_received);
    LONGS_EQUAL(0, block_listener.bytes_received);
    LONGS_EQUAL(0, block_listener.last_block_info.more);
    LONGS_EQUAL(GG_COAP_MESSAGE_CODE_FORBIDDEN, block_listener.last_code);
}

//-----------------------------------------------------------------------
// Test a blockwise PUT request to a handler that doesn't handle blockwise
//-----------------------------------------------------------------------
TEST(GG_COAP_BLOCKWISE, Test_SimpleNonBlockwiseServer) {
    BlockListener block_listener = {
        GG_INTERFACE_INITIALIZER(BlockListener, GG_CoapBlockwiseResponseListener)
    };

    Handler4 handler = {
        GG_INTERFACE_INITIALIZER(Handler4, GG_CoapRequestHandler)
    };
    Handler4Item items[] = {
        {
            .payload_size  = 0,
            .response_code = GG_COAP_MESSAGE_CODE_CONTENT,
            .option        = 0
        }
    };
    handler.items = items;
    handler.item_count = GG_ARRAY_SIZE(items);

    // create a block source
    BlockSource block_source = {
        GG_INTERFACE_INITIALIZER(BlockSource, GG_CoapBlockSource),
        .payload_size = 123
    };

    Handler4_TestRun(&handler,
                     GG_COAP_REQUEST_HANDLER_FLAG_ALLOW_PUT,
                     GG_CAST(&block_source, GG_CoapBlockSource),
                     GG_CAST(&block_listener, GG_CoapBlockwiseResponseListener),
                     GG_COAP_METHOD_PUT,
                     0);

    LONGS_EQUAL(GG_SUCCESS, block_listener.last_error);
    LONGS_EQUAL(1, block_listener.blocks_received);
    LONGS_EQUAL(0, block_listener.bytes_received);
    LONGS_EQUAL(GG_COAP_MESSAGE_CODE_CONTENT, block_listener.last_code);
}

//-----------------------------------------------------------------------
// Test a blockwise PUT request to a handler that returns CONTINUE but
// no BLOCK1
//-----------------------------------------------------------------------
TEST(GG_COAP_BLOCKWISE, Test_ContinueWithoutBlock1) {
    BlockListener block_listener = {
        GG_INTERFACE_INITIALIZER(BlockListener, GG_CoapBlockwiseResponseListener)
    };

    Handler4 handler = {
        GG_INTERFACE_INITIALIZER(Handler4, GG_CoapRequestHandler)
    };
    Handler4Item items[] = {
        {
            .payload_size  = 0,
            .response_code = GG_COAP_MESSAGE_CODE_CONTINUE,
            .option        = 0
        }
    };
    handler.items = items;
    handler.item_count = GG_ARRAY_SIZE(items);

    // create a block source
    BlockSource block_source = {
        GG_INTERFACE_INITIALIZER(BlockSource, GG_CoapBlockSource),
        .payload_size = 10000
    };

    Handler4_TestRun(&handler,
                     GG_COAP_REQUEST_HANDLER_FLAG_ALLOW_PUT,
                     GG_CAST(&block_source, GG_CoapBlockSource),
                     GG_CAST(&block_listener, GG_CoapBlockwiseResponseListener),
                     GG_COAP_METHOD_PUT,
                     0);

    LONGS_EQUAL(GG_ERROR_COAP_INVALID_RESPONSE, block_listener.last_error);
    LONGS_EQUAL(0, block_listener.blocks_received);
    LONGS_EQUAL(0, block_listener.bytes_received);
}

//-----------------------------------------------------------------------
// Test a blockwise GET request to a handler that returns the wrong block
//-----------------------------------------------------------------------
TEST(GG_COAP_BLOCKWISE, Test_GetWithWrongBlockResponse) {
    BlockListener block_listener = {
        GG_INTERFACE_INITIALIZER(BlockListener, GG_CoapBlockwiseResponseListener)
    };

    Handler4 handler = {
        GG_INTERFACE_INITIALIZER(Handler4, GG_CoapRequestHandler)
    };
    Handler4Item items[] = {
        {
            .payload_size  = 123,
            .response_code = GG_COAP_MESSAGE_CODE_CONTENT,
            .option        = GG_COAP_MESSAGE_OPTION_BLOCK2,
            .block_info = {
                .offset = 1024,
                .size   = 1024,
                .more   = true
            }
        }
    };
    handler.items = items;
    handler.item_count = GG_ARRAY_SIZE(items);

    Handler4_TestRun(&handler,
                     GG_COAP_REQUEST_HANDLER_FLAG_ALLOW_GET,
                     NULL,
                     GG_CAST(&block_listener, GG_CoapBlockwiseResponseListener),
                     GG_COAP_METHOD_GET,
                     0);

    LONGS_EQUAL(GG_ERROR_COAP_UNEXPECTED_BLOCK, block_listener.last_error);
    LONGS_EQUAL(0, block_listener.blocks_received);
    LONGS_EQUAL(0, block_listener.bytes_received);
}

//-----------------------------------------------------------------------
// Test a blockwise GET request with a server that changes the block size
//-----------------------------------------------------------------------
TEST(GG_COAP_BLOCKWISE, Test_GetWithServerImposedBlockSize) {
    BlockListener block_listener = {
        GG_INTERFACE_INITIALIZER(BlockListener, GG_CoapBlockwiseResponseListener)
    };

    Handler4 handler = {
        GG_INTERFACE_INITIALIZER(Handler4, GG_CoapRequestHandler)
    };
    Handler4Item items[] = {
        {
            .payload_size  = 64,
            .response_code = GG_COAP_MESSAGE_CODE_CONTENT,
            .option        = GG_COAP_MESSAGE_OPTION_BLOCK2,
            .block_info = {
                .offset = 0,
                .size   = 64,
                .more   = true
            }
        },
        {
            .payload_size  = 30,
            .response_code = GG_COAP_MESSAGE_CODE_CONTENT,
            .option        = GG_COAP_MESSAGE_OPTION_BLOCK2,
            .block_info = {
                .offset = 64,
                .size   = 64,
                .more   = false
            }
        }
    };
    handler.items = items;
    handler.item_count = GG_ARRAY_SIZE(items);
    GG_CoapMessageBlockInfo block2_infos[GG_ARRAY_SIZE(items)];
    handler.received_block2_options = block2_infos;

    Handler4_TestRun(&handler,
                     GG_COAP_REQUEST_HANDLER_FLAG_ALLOW_GET,
                     NULL,
                     GG_CAST(&block_listener, GG_CoapBlockwiseResponseListener),
                     GG_COAP_METHOD_GET,
                     1024);

    LONGS_EQUAL(GG_SUCCESS, block_listener.last_error);
    LONGS_EQUAL(2, block_listener.blocks_received);
    LONGS_EQUAL(94, block_listener.bytes_received);
    LONGS_EQUAL(1024, handler.received_block2_options[0].size);
    LONGS_EQUAL(64, handler.received_block2_options[1].size);
    LONGS_EQUAL(64, handler.received_block2_options[1].offset);
}

//-----------------------------------------------------------------------
// Test a blockwise GET request with a server that doesn't return a BLOCK2 option on the second block
//-----------------------------------------------------------------------
TEST(GG_COAP_BLOCKWISE, Test_GetWithMissingBlock2) {
    BlockListener block_listener = {
        GG_INTERFACE_INITIALIZER(BlockListener, GG_CoapBlockwiseResponseListener)
    };

    Handler4 handler = {
        GG_INTERFACE_INITIALIZER(Handler4, GG_CoapRequestHandler)
    };
    Handler4Item items[] = {
        {
            .payload_size  = 64,
            .response_code = GG_COAP_MESSAGE_CODE_CONTENT,
            .option        = GG_COAP_MESSAGE_OPTION_BLOCK2,
            .block_info = {
                .offset = 0,
                .size   = 64,
                .more   = true
            }
        },
        {
            .payload_size  = 30,
            .response_code = GG_COAP_MESSAGE_CODE_CONTENT,
            .option        = 0
        }
    };
    handler.items = items;
    handler.item_count = GG_ARRAY_SIZE(items);

    Handler4_TestRun(&handler,
                     GG_COAP_REQUEST_HANDLER_FLAG_ALLOW_GET,
                     NULL,
                     GG_CAST(&block_listener, GG_CoapBlockwiseResponseListener),
                     GG_COAP_METHOD_GET,
                     1024);

    LONGS_EQUAL(GG_ERROR_COAP_INVALID_RESPONSE, block_listener.last_error);
    LONGS_EQUAL(1, block_listener.blocks_received);
    LONGS_EQUAL(64, block_listener.bytes_received);
}

//-----------------------------------------------------------------------
// Test a blockwise PUT request to a handler that changes the block size
//-----------------------------------------------------------------------
TEST(GG_COAP_BLOCKWISE, Test_PutWithServerImposedBlockSize) {
    BlockListener block_listener = {
        GG_INTERFACE_INITIALIZER(BlockListener, GG_CoapBlockwiseResponseListener)
    };

    Handler4 handler = {
        GG_INTERFACE_INITIALIZER(Handler4, GG_CoapRequestHandler)
    };
    Handler4Item items[] = {
        {
            .payload_size  = 0,
            .response_code = GG_COAP_MESSAGE_CODE_CONTINUE,
            .option        = GG_COAP_MESSAGE_OPTION_BLOCK1,
            .block_info = {
                .offset = 0,
                .size   = 64,
                .more   = true
            }
        },
        {
            .payload_size  = 0,
            .response_code = GG_COAP_MESSAGE_CODE_CONTENT,
            .option        = GG_COAP_MESSAGE_OPTION_BLOCK1,
            .block_info = {
                .offset = 1024,
                .size   = 64,
                .more   = false
            }
        }
    };
    handler.items = items;
    handler.item_count = GG_ARRAY_SIZE(items);
    GG_CoapMessageBlockInfo block1_infos[GG_ARRAY_SIZE(items)];
    handler.received_block1_options = block1_infos;

    // create a block source
    BlockSource block_source = {
        GG_INTERFACE_INITIALIZER(BlockSource, GG_CoapBlockSource),
        .payload_size = 1024+30
    };

    Handler4_TestRun(&handler,
                     GG_COAP_REQUEST_HANDLER_FLAG_ALLOW_PUT,
                     GG_CAST(&block_source, GG_CoapBlockSource),
                     GG_CAST(&block_listener, GG_CoapBlockwiseResponseListener),
                     GG_COAP_METHOD_PUT,
                     1024);

    LONGS_EQUAL(1, block_listener.blocks_received);
    LONGS_EQUAL(0, block_listener.bytes_received);
    LONGS_EQUAL(GG_COAP_MESSAGE_CODE_CONTENT, block_listener.last_code);
    LONGS_EQUAL(1024+30, handler.bytes_received);
    LONGS_EQUAL(2, handler.blocks_received);

    LONGS_EQUAL(0, block1_infos[0].offset);
    LONGS_EQUAL(1024, block1_infos[0].size);
    LONGS_EQUAL(1, block1_infos[0].more);

    LONGS_EQUAL(1024, block1_infos[1].offset);
    LONGS_EQUAL(64, block1_infos[1].size);
    LONGS_EQUAL(0, block1_infos[1].more);
}

//-----------------------------------------------------------------------
// Test a blockwise POST request with an empty payload represented by a NULL source
//-----------------------------------------------------------------------
TEST(GG_COAP_BLOCKWISE, Test_PostWithNullSource) {
    BlockListener block_listener = {
        GG_INTERFACE_INITIALIZER(BlockListener, GG_CoapBlockwiseResponseListener)
    };

    Handler4 handler = {
        GG_INTERFACE_INITIALIZER(Handler4, GG_CoapRequestHandler)
    };
    Handler4Item items[] = {
        {
            .payload_size  = 0,
            .response_code = GG_COAP_MESSAGE_CODE_CONTENT,
            .option        = GG_COAP_MESSAGE_OPTION_BLOCK1,
            .block_info = {
                .offset = 0,
                .size   = 64,
                .more   = false
            }
        }
    };
    handler.items = items;
    handler.item_count = GG_ARRAY_SIZE(items);

    Handler4_TestRun(&handler,
                     GG_COAP_REQUEST_HANDLER_FLAG_ALLOW_POST,
                     NULL,
                     GG_CAST(&block_listener, GG_CoapBlockwiseResponseListener),
                     GG_COAP_METHOD_POST,
                     0);

    LONGS_EQUAL(GG_SUCCESS, block_listener.last_error);
    LONGS_EQUAL(1, block_listener.blocks_received);
    LONGS_EQUAL(0, block_listener.bytes_received);
    LONGS_EQUAL(GG_COAP_MESSAGE_CODE_CONTENT, block_listener.last_code);
    LONGS_EQUAL(1, handler.blocks_received);
    LONGS_EQUAL(0, handler.bytes_received);
}

TEST(GG_COAP_BLOCKWISE, Test_ApiEdgeCases) {
    size_t block_size = 1024;
    bool more = false;
    GG_Result result = GG_CoapMessageBlockInfo_AdjustAndGetChunkSize(2000, &block_size, &more, 1000);
    LONGS_EQUAL(GG_ERROR_OUT_OF_RANGE, result);

    GG_CoapMessage* message;
    GG_CoapMessageOptionParam option = GG_COAP_MESSAGE_OPTION_PARAM_UINT(BLOCK1, 7);
    result = GG_CoapMessage_Create(0, GG_COAP_MESSAGE_TYPE_CON, &option, 1, 0, NULL, 0, NULL, 0, &message);
    LONGS_EQUAL(GG_SUCCESS, result);
    GG_CoapMessageBlockInfo block_info;
    result = GG_CoapMessage_GetBlockInfo(message, GG_COAP_MESSAGE_OPTION_BLOCK1, &block_info, 1024);
    LONGS_EQUAL(GG_ERROR_INVALID_FORMAT, result);
    GG_CoapMessage_Destroy(message);
}

TEST(GG_COAP_BLOCKWISE, Test_ServerBlockwiseHelper_BLOCK1) {
    GG_CoapBlockwiseServerHelper helper;

    GG_CoapBlockwiseServerHelper_Init(&helper, GG_COAP_MESSAGE_OPTION_BLOCK1, 0);

    GG_CoapMessage* request = NULL;
    uint8_t token[1] =  { 0 };
    uint8_t payload[1024];
    memset(payload, 0, sizeof(payload));

    // create a message for a block with offset 0
    GG_CoapMessageBlockInfo block_info;
    block_info.offset = 0;
    block_info.size = 1024;
    block_info.more = true;
    uint32_t block_option_value;
    GG_CoapMessageBlockInfo_ToOptionValue(&block_info, &block_option_value);
    GG_CoapMessageOptionParam block_option = GG_COAP_MESSAGE_OPTION_PARAM_UINT(BLOCK1, block_option_value);
    GG_Result result = GG_CoapMessage_Create(GG_COAP_METHOD_GET,
                                             GG_COAP_MESSAGE_TYPE_CON,
                                             &block_option,
                                             1,
                                             0,
                                             token,
                                             sizeof(token),
                                             payload,
                                             sizeof(payload),
                                             &request);
    LONGS_EQUAL(GG_SUCCESS, result);

    // process the block 0 request
    bool was_resent = false;
    result = GG_CoapBlockwiseServerHelper_OnRequest(&helper, request, &was_resent);
    LONGS_EQUAL(GG_SUCCESS, result);
    LONGS_EQUAL(0, (int)was_resent);
    LONGS_EQUAL(helper.next_offset, 1024);
    LONGS_EQUAL(0, (int)helper.done);

    // cleanup
    GG_CoapMessage_Destroy(request);

    // create a message for a block with offset out of range
    block_info.offset = 10000;
    block_info.size = 1024;
    block_info.more = true;
    GG_CoapMessageBlockInfo_ToOptionValue(&block_info, &block_option_value);
    block_option = (GG_CoapMessageOptionParam) GG_COAP_MESSAGE_OPTION_PARAM_UINT(BLOCK1, block_option_value);
    result = GG_CoapMessage_Create(GG_COAP_METHOD_GET,
                                   GG_COAP_MESSAGE_TYPE_CON,
                                   &block_option,
                                   1,
                                   0,
                                   token,
                                   sizeof(token),
                                   payload,
                                   sizeof(payload),
                                   &request);
    LONGS_EQUAL(GG_SUCCESS, result);

    // process the out of range block, it should fail
    result = GG_CoapBlockwiseServerHelper_OnRequest(&helper, request, &was_resent);
    LONGS_EQUAL(GG_COAP_MESSAGE_CODE_REQUEST_ENTITY_INCOMPLETE, result);
    LONGS_EQUAL(0, (int)was_resent);
    LONGS_EQUAL(helper.next_offset, 1024);
    LONGS_EQUAL(0, (int)helper.done);

    // cleanup
    GG_CoapMessage_Destroy(request);

    // create a message for block 1
    block_info.offset = 1024;
    block_info.size = 1024;
    block_info.more = true;
    GG_CoapMessageBlockInfo_ToOptionValue(&block_info, &block_option_value);
    block_option = (GG_CoapMessageOptionParam) GG_COAP_MESSAGE_OPTION_PARAM_UINT(BLOCK1, block_option_value);
    result = GG_CoapMessage_Create(GG_COAP_METHOD_GET,
                                   GG_COAP_MESSAGE_TYPE_CON,
                                   &block_option,
                                   1,
                                   0,
                                   token,
                                   sizeof(token),
                                   payload,
                                   sizeof(payload),
                                   &request);
    LONGS_EQUAL(GG_SUCCESS, result);

    // process the block 1 request
    was_resent = false;
    result = GG_CoapBlockwiseServerHelper_OnRequest(&helper, request, &was_resent);
    LONGS_EQUAL(GG_SUCCESS, result);
    LONGS_EQUAL(0, (int)was_resent);
    LONGS_EQUAL(0, (int)helper.done);
    LONGS_EQUAL(helper.next_offset, 2048);

    // process the same block again and check that it was marked as resent
    was_resent = false;
    result = GG_CoapBlockwiseServerHelper_OnRequest(&helper, request, &was_resent);
    LONGS_EQUAL(GG_SUCCESS, result);
    LONGS_EQUAL(1, (int)was_resent);
    LONGS_EQUAL(0, (int)helper.done);
    LONGS_EQUAL(helper.next_offset, 2048);

    // cleanup
    GG_CoapMessage_Destroy(request);

    // create a message for block 3 (not sequential)
    block_info.offset = 3072;
    block_info.size = 1024;
    block_info.more = false;
    GG_CoapMessageBlockInfo_ToOptionValue(&block_info, &block_option_value);
    block_option = (GG_CoapMessageOptionParam) GG_COAP_MESSAGE_OPTION_PARAM_UINT(BLOCK1, block_option_value);
    result = GG_CoapMessage_Create(GG_COAP_METHOD_GET,
                                   GG_COAP_MESSAGE_TYPE_CON,
                                   &block_option,
                                   1,
                                   0,
                                   token,
                                   sizeof(token),
                                   payload,
                                   sizeof(payload),
                                   &request);
    LONGS_EQUAL(GG_SUCCESS, result);

    // process the block 3 request
    was_resent = false;
    result = GG_CoapBlockwiseServerHelper_OnRequest(&helper, request, &was_resent);
    LONGS_EQUAL(GG_COAP_MESSAGE_CODE_REQUEST_ENTITY_INCOMPLETE, result);

    // create a message for block 2
    block_info.offset = 2048;
    block_info.size = 1024;
    block_info.more = false;
    GG_CoapMessageBlockInfo_ToOptionValue(&block_info, &block_option_value);
    block_option = (GG_CoapMessageOptionParam) GG_COAP_MESSAGE_OPTION_PARAM_UINT(BLOCK1, block_option_value);
    result = GG_CoapMessage_Create(GG_COAP_METHOD_GET,
                                   GG_COAP_MESSAGE_TYPE_CON,
                                   &block_option,
                                   1,
                                   0,
                                   token,
                                   sizeof(token),
                                   payload,
                                   sizeof(payload),
                                   &request);
    LONGS_EQUAL(GG_SUCCESS, result);

    // process the block 2 request
    was_resent = false;
    result = GG_CoapBlockwiseServerHelper_OnRequest(&helper, request, &was_resent);
    LONGS_EQUAL(GG_SUCCESS, result);
    LONGS_EQUAL(0, (int)was_resent);
    LONGS_EQUAL(1, (int)helper.done);
    LONGS_EQUAL(helper.next_offset, 2048);

    // process the same block and check that it is signaled as resent
    was_resent = false;
    result = GG_CoapBlockwiseServerHelper_OnRequest(&helper, request, &was_resent);
    LONGS_EQUAL(GG_SUCCESS, result);
    LONGS_EQUAL(1, (int)was_resent);
    LONGS_EQUAL(1, (int)helper.done);
    LONGS_EQUAL(helper.next_offset, 2048);

    // cleanup
    GG_CoapMessage_Destroy(request);

    // create a message for another block 2
    block_info.offset = 2048;
    block_info.size = 1024;
    block_info.more = true;
    GG_CoapMessageBlockInfo_ToOptionValue(&block_info, &block_option_value);
    block_option = (GG_CoapMessageOptionParam) GG_COAP_MESSAGE_OPTION_PARAM_UINT(BLOCK1, block_option_value);
    result = GG_CoapMessage_Create(GG_COAP_METHOD_GET,
                                   GG_COAP_MESSAGE_TYPE_CON,
                                   &block_option,
                                   1,
                                   0,
                                   token,
                                   sizeof(token),
                                   payload,
                                   sizeof(payload),
                                   &request);
    LONGS_EQUAL(GG_SUCCESS, result);

    // process the block request, should be rejected because the previous block was a final block but this one isn't
    was_resent = false;
    result = GG_CoapBlockwiseServerHelper_OnRequest(&helper, request, &was_resent);
    CHECK_TRUE(GG_FAILED(result));

    // cleanup
    GG_CoapMessage_Destroy(request);

    // create a message for block 0
    block_info.offset = 0;
    block_info.size = 1024;
    block_info.more = true;
    GG_CoapMessageBlockInfo_ToOptionValue(&block_info, &block_option_value);
    block_option = (GG_CoapMessageOptionParam) GG_COAP_MESSAGE_OPTION_PARAM_UINT(BLOCK1, block_option_value);
    result = GG_CoapMessage_Create(GG_COAP_METHOD_GET,
                                   GG_COAP_MESSAGE_TYPE_CON,
                                   &block_option,
                                   1,
                                   0,
                                   token,
                                   sizeof(token),
                                   payload,
                                   sizeof(payload),
                                   &request);
    LONGS_EQUAL(GG_SUCCESS, result);

    // process the block 0 request, which should start a new transfer
    was_resent = false;
    result = GG_CoapBlockwiseServerHelper_OnRequest(&helper, request, &was_resent);
    LONGS_EQUAL(GG_SUCCESS, result);
    LONGS_EQUAL(0, (int)was_resent);
    LONGS_EQUAL(0, (int)helper.done);

    // cleanup
    GG_CoapMessage_Destroy(request);

    // set an ETag for the transfer
    uint8_t etag1[3] = { 0x01, 0x02, 0x03 };
    GG_CoapBlockwiseServerHelper_SetEtag(&helper, etag1, sizeof(etag1));

    // create a message for block 0, with an If-Match option
    GG_CoapMessageOptionParam etag_option = GG_COAP_MESSAGE_OPTION_PARAM_OPAQUE(IF_MATCH, etag1, sizeof(etag1));
    result = GG_CoapMessage_Create(GG_COAP_METHOD_GET,
                                   GG_COAP_MESSAGE_TYPE_CON,
                                   &etag_option,
                                   1,
                                   0,
                                   token,
                                   sizeof(token),
                                   payload,
                                   sizeof(payload),
                                   &request);
    LONGS_EQUAL(GG_SUCCESS, result);

    // process the block 0 request, which should start a new transfer
    was_resent = false;
    result = GG_CoapBlockwiseServerHelper_OnRequest(&helper, request, &was_resent);
    LONGS_EQUAL(GG_SUCCESS, result);
    LONGS_EQUAL(0, (int)was_resent);
    LONGS_EQUAL(1, (int)helper.done);

    // set a different ETag for the transfer
    uint8_t etag2[3] = { 0x04, 0x05, 0x06 };
    GG_CoapBlockwiseServerHelper_SetEtag(&helper, etag2, sizeof(etag2));

    // process the block 0 request again, this time it should fail with an ETag mismatch
    was_resent = false;
    result = GG_CoapBlockwiseServerHelper_OnRequest(&helper, request, &was_resent);
    LONGS_EQUAL(GG_COAP_MESSAGE_CODE_PRECONDITION_FAILED, result);
    LONGS_EQUAL(0, (int)was_resent);

    // now set a matching Etag and try again
    GG_CoapBlockwiseServerHelper_SetEtag(&helper, etag1, sizeof(etag1));
    was_resent = false;
    result = GG_CoapBlockwiseServerHelper_OnRequest(&helper, request, &was_resent);
    LONGS_EQUAL(GG_SUCCESS, result);
    LONGS_EQUAL(0, (int)was_resent);

    // cleanup
    GG_CoapMessage_Destroy(request);

    // create an endpoint
    GG_CoapEndpoint* endpoint = NULL;
    GG_TimerScheduler* timer_scheduler = NULL;
    result = GG_TimerScheduler_Create(&timer_scheduler);
    LONGS_EQUAL(GG_SUCCESS, result);
    result = GG_CoapEndpoint_Create(timer_scheduler, NULL, NULL, &endpoint);
    LONGS_EQUAL(GG_SUCCESS, result);

    // create a message for block 0
    block_info.offset = 0;
    block_info.size = 1024;
    block_info.more = true;
    GG_CoapMessageBlockInfo_ToOptionValue(&block_info, &block_option_value);
    block_option = (GG_CoapMessageOptionParam) GG_COAP_MESSAGE_OPTION_PARAM_UINT(BLOCK1, block_option_value);
    result = GG_CoapMessage_Create(GG_COAP_METHOD_GET,
                                   GG_COAP_MESSAGE_TYPE_CON,
                                   &block_option,
                                   1,
                                   0,
                                   token,
                                   sizeof(token),
                                   payload,
                                   sizeof(payload),
                                   &request);
    LONGS_EQUAL(GG_SUCCESS, result);

    // ask the helper to create a response
    GG_CoapMessage* response = NULL;
    result = GG_CoapBlockwiseServerHelper_CreateResponse(&helper,
                                                         endpoint,
                                                         request,
                                                         GG_COAP_MESSAGE_CODE_CREATED,
                                                         NULL,
                                                         0,
                                                         NULL,
                                                         0,
                                                         &response);
    LONGS_EQUAL(GG_SUCCESS, result);

    GG_CoapMessageOption option;
    result = GG_CoapMessage_GetOption(response, GG_COAP_MESSAGE_OPTION_BLOCK1, &option, 0);
    LONGS_EQUAL(GG_SUCCESS, result);
    result = GG_CoapMessage_GetBlockInfo(response, GG_COAP_MESSAGE_OPTION_BLOCK1, &block_info, 0);
    LONGS_EQUAL(GG_SUCCESS, result);
    LONGS_EQUAL(0, (int)block_info.more);
    LONGS_EQUAL(0, block_info.offset);

    // cleanup
    GG_CoapMessage_Destroy(request);
    GG_CoapMessage_Destroy(response);
    GG_CoapEndpoint_Destroy(endpoint);
    GG_TimerScheduler_Destroy(timer_scheduler);
}

TEST(GG_COAP_BLOCKWISE, Test_ServerBlockwiseHelper_BLOCK2) {
    GG_CoapBlockwiseServerHelper helper;

    GG_CoapBlockwiseServerHelper_Init(&helper, GG_COAP_MESSAGE_OPTION_BLOCK2, 0);

    GG_CoapMessage* request = NULL;
    uint8_t token[1] =  { 0 };

    // create a message for a block request with offset 0
    GG_CoapMessageBlockInfo block_info;
    block_info.offset = 0;
    block_info.size = 1024;
    block_info.more = false;
    uint32_t block_option_value;
    GG_CoapMessageBlockInfo_ToOptionValue(&block_info, &block_option_value);
    GG_CoapMessageOptionParam block_option = GG_COAP_MESSAGE_OPTION_PARAM_UINT(BLOCK2, block_option_value);
    GG_Result result = GG_CoapMessage_Create(GG_COAP_METHOD_GET,
                                             GG_COAP_MESSAGE_TYPE_CON,
                                             &block_option,
                                             1,
                                             0,
                                             token,
                                             sizeof(token),
                                             NULL,
                                             0,
                                             &request);
    LONGS_EQUAL(GG_SUCCESS, result);

    // process the block 0 request
    bool was_resent = false;
    result = GG_CoapBlockwiseServerHelper_OnRequest(&helper, request, &was_resent);
    LONGS_EQUAL(GG_SUCCESS, result);
    LONGS_EQUAL(0, (int)was_resent);
    LONGS_EQUAL(helper.next_offset, 1024);
    LONGS_EQUAL(0, (int)helper.done);

    // process the block 0 request again
    result = GG_CoapBlockwiseServerHelper_OnRequest(&helper, request, &was_resent);
    LONGS_EQUAL(GG_SUCCESS, result);
    LONGS_EQUAL(0, (int)was_resent);
    LONGS_EQUAL(helper.next_offset, 1024);
    LONGS_EQUAL(0, (int)helper.done);

    // cleanup
    GG_CoapMessage_Destroy(request);

    // create a message for a block with offset out of range
    block_info.offset = 10000;
    block_info.size = 1024;
    block_info.more = true;
    GG_CoapMessageBlockInfo_ToOptionValue(&block_info, &block_option_value);
    block_option = (GG_CoapMessageOptionParam) GG_COAP_MESSAGE_OPTION_PARAM_UINT(BLOCK2, block_option_value);
    result = GG_CoapMessage_Create(GG_COAP_METHOD_GET,
                                   GG_COAP_MESSAGE_TYPE_CON,
                                   &block_option,
                                   1,
                                   0,
                                   token,
                                   sizeof(token),
                                   NULL,
                                   0,
                                   &request);
    LONGS_EQUAL(GG_SUCCESS, result);

    // process the out of range block, it should fail
    result = GG_CoapBlockwiseServerHelper_OnRequest(&helper, request, &was_resent);
    LONGS_EQUAL(GG_COAP_MESSAGE_CODE_PRECONDITION_FAILED, result);
    LONGS_EQUAL(0, (int)was_resent);
    LONGS_EQUAL(helper.next_offset, 1024);
    LONGS_EQUAL(0, (int)helper.done);

    // cleanup
    GG_CoapMessage_Destroy(request);
    
    // create a message for block 1
    block_info.offset = 1024;
    block_info.size = 1024;
    block_info.more = true;
    GG_CoapMessageBlockInfo_ToOptionValue(&block_info, &block_option_value);
    block_option = (GG_CoapMessageOptionParam) GG_COAP_MESSAGE_OPTION_PARAM_UINT(BLOCK2, block_option_value);
    result = GG_CoapMessage_Create(GG_COAP_METHOD_GET,
                                   GG_COAP_MESSAGE_TYPE_CON,
                                   &block_option,
                                   1,
                                   0,
                                   token,
                                   sizeof(token),
                                   NULL,
                                   0,
                                   &request);
    LONGS_EQUAL(GG_SUCCESS, result);

    // process the block 1 request
    was_resent = false;
    result = GG_CoapBlockwiseServerHelper_OnRequest(&helper, request, &was_resent);
    LONGS_EQUAL(GG_SUCCESS, result);
    LONGS_EQUAL(0, (int)was_resent);
    LONGS_EQUAL(0, (int)helper.done);
    LONGS_EQUAL(helper.next_offset, 2048);

    // process the same block again and check that it was marked as resent
    was_resent = false;
    result = GG_CoapBlockwiseServerHelper_OnRequest(&helper, request, &was_resent);
    LONGS_EQUAL(GG_SUCCESS, result);
    LONGS_EQUAL(1, (int)was_resent);
    LONGS_EQUAL(0, (int)helper.done);
    LONGS_EQUAL(helper.next_offset, 2048);

    // cleanup
    GG_CoapMessage_Destroy(request);

    // create a message for block 3 (not sequential)
    block_info.offset = 3072;
    block_info.size = 1024;
    block_info.more = false;
    GG_CoapMessageBlockInfo_ToOptionValue(&block_info, &block_option_value);
    block_option = (GG_CoapMessageOptionParam) GG_COAP_MESSAGE_OPTION_PARAM_UINT(BLOCK2, block_option_value);
    result = GG_CoapMessage_Create(GG_COAP_METHOD_GET,
                                   GG_COAP_MESSAGE_TYPE_CON,
                                   &block_option,
                                   1,
                                   0,
                                   token,
                                   sizeof(token),
                                   NULL,
                                   0,
                                   &request);
    LONGS_EQUAL(GG_SUCCESS, result);

    // process the block 3 request
    was_resent = false;
    result = GG_CoapBlockwiseServerHelper_OnRequest(&helper, request, &was_resent);
    LONGS_EQUAL(GG_COAP_MESSAGE_CODE_PRECONDITION_FAILED, result);

    // create a message for block 2
    block_info.offset = 2048;
    block_info.size = 1024;
    block_info.more = false;
    GG_CoapMessageBlockInfo_ToOptionValue(&block_info, &block_option_value);
    block_option = (GG_CoapMessageOptionParam) GG_COAP_MESSAGE_OPTION_PARAM_UINT(BLOCK2, block_option_value);
    result = GG_CoapMessage_Create(GG_COAP_METHOD_GET,
                                   GG_COAP_MESSAGE_TYPE_CON,
                                   &block_option,
                                   1,
                                   0,
                                   token,
                                   sizeof(token),
                                   NULL,
                                   0,
                                   &request);
    LONGS_EQUAL(GG_SUCCESS, result);

    // process the block 2 request
    was_resent = false;
    result = GG_CoapBlockwiseServerHelper_OnRequest(&helper, request, &was_resent);
    LONGS_EQUAL(GG_SUCCESS, result);
    LONGS_EQUAL(0, (int)was_resent);
    LONGS_EQUAL(0, (int)helper.done);
    LONGS_EQUAL(helper.next_offset, 3072);

    // process the same block and check that it is signaled as resent
    was_resent = false;
    result = GG_CoapBlockwiseServerHelper_OnRequest(&helper, request, &was_resent);
    LONGS_EQUAL(GG_SUCCESS, result);
    LONGS_EQUAL(1, (int)was_resent);
    LONGS_EQUAL(0, (int)helper.done);
    LONGS_EQUAL(helper.next_offset, 3072);

    // cleanup
    GG_CoapMessage_Destroy(request);

    // create a message for block 0
    block_info.offset = 0;
    block_info.size = 1024;
    block_info.more = true;
    GG_CoapMessageBlockInfo_ToOptionValue(&block_info, &block_option_value);
    block_option = (GG_CoapMessageOptionParam) GG_COAP_MESSAGE_OPTION_PARAM_UINT(BLOCK2, block_option_value);
    result = GG_CoapMessage_Create(GG_COAP_METHOD_GET,
                                   GG_COAP_MESSAGE_TYPE_CON,
                                   &block_option,
                                   1,
                                   0,
                                   token,
                                   sizeof(token),
                                   NULL,
                                   0,
                                   &request);
    LONGS_EQUAL(GG_SUCCESS, result);

    // process the block 0 request, which should start a new transfer
    was_resent = false;
    result = GG_CoapBlockwiseServerHelper_OnRequest(&helper, request, &was_resent);
    LONGS_EQUAL(GG_SUCCESS, result);
    LONGS_EQUAL(0, (int)was_resent);
    LONGS_EQUAL(0, (int)helper.done);

    // cleanup
    GG_CoapMessage_Destroy(request);

    // set an ETag for the transfer
    uint8_t etag1[3] = { 0x01, 0x02, 0x03 };
    GG_CoapBlockwiseServerHelper_SetEtag(&helper, etag1, sizeof(etag1));

    // create a message for block 0, with an If-Match option
    GG_CoapMessageOptionParam etag_option = GG_COAP_MESSAGE_OPTION_PARAM_OPAQUE(IF_MATCH, etag1, sizeof(etag1));
    result = GG_CoapMessage_Create(GG_COAP_METHOD_GET,
                                   GG_COAP_MESSAGE_TYPE_CON,
                                   &etag_option,
                                   1,
                                   0,
                                   token,
                                   sizeof(token),
                                   NULL,
                                   0,
                                   &request);
    LONGS_EQUAL(GG_SUCCESS, result);

    // process the block 0 request, which should start a new transfer
    was_resent = false;
    result = GG_CoapBlockwiseServerHelper_OnRequest(&helper, request, &was_resent);
    LONGS_EQUAL(GG_SUCCESS, result);
    LONGS_EQUAL(0, (int)was_resent);
    LONGS_EQUAL(0, (int)helper.done);

    // set a different ETag for the transfer
    uint8_t etag2[3] = { 0x04, 0x05, 0x06 };
    GG_CoapBlockwiseServerHelper_SetEtag(&helper, etag2, sizeof(etag2));

    // process the block 0 request again, this time it should fail with an ETag mismatch
    was_resent = false;
    result = GG_CoapBlockwiseServerHelper_OnRequest(&helper, request, &was_resent);
    LONGS_EQUAL(GG_COAP_MESSAGE_CODE_PRECONDITION_FAILED, result);
    LONGS_EQUAL(0, (int)was_resent);

    // now set a matching Etag and try again
    GG_CoapBlockwiseServerHelper_SetEtag(&helper, etag1, sizeof(etag1));
    was_resent = false;
    result = GG_CoapBlockwiseServerHelper_OnRequest(&helper, request, &was_resent);
    LONGS_EQUAL(GG_SUCCESS, result);
    LONGS_EQUAL(0, (int)was_resent);

    // cleanup
    GG_CoapMessage_Destroy(request);

    // create an endpoint
    GG_CoapEndpoint* endpoint = NULL;
    GG_TimerScheduler* timer_scheduler = NULL;
    result = GG_TimerScheduler_Create(&timer_scheduler);
    LONGS_EQUAL(GG_SUCCESS, result);
    result = GG_CoapEndpoint_Create(timer_scheduler, NULL, NULL, &endpoint);
    LONGS_EQUAL(GG_SUCCESS, result);

    // create a message for block 0
    block_info.offset = 0;
    block_info.size = 1024;
    block_info.more = true;
    GG_CoapMessageBlockInfo_ToOptionValue(&block_info, &block_option_value);
    block_option = (GG_CoapMessageOptionParam) GG_COAP_MESSAGE_OPTION_PARAM_UINT(BLOCK2, block_option_value);
    result = GG_CoapMessage_Create(GG_COAP_METHOD_GET,
                                   GG_COAP_MESSAGE_TYPE_CON,
                                   &block_option,
                                   1,
                                   0,
                                   token,
                                   sizeof(token),
                                   NULL,
                                   0,
                                   &request);
    LONGS_EQUAL(GG_SUCCESS, result);

    // ask the helper to create a response
    GG_CoapMessage* response = NULL;
    result = GG_CoapBlockwiseServerHelper_CreateResponse(&helper,
                                                         endpoint,
                                                         request,
                                                         GG_COAP_MESSAGE_CODE_CREATED,
                                                         NULL,
                                                         0,
                                                         NULL,
                                                         0,
                                                         &response);
    LONGS_EQUAL(GG_SUCCESS, result);

    GG_CoapMessageOption option;
    result = GG_CoapMessage_GetOption(response, GG_COAP_MESSAGE_OPTION_BLOCK2, &option, 0);
    LONGS_EQUAL(GG_SUCCESS, result);
    result = GG_CoapMessage_GetBlockInfo(response, GG_COAP_MESSAGE_OPTION_BLOCK2, &block_info, 0);
    LONGS_EQUAL(GG_SUCCESS, result);
    LONGS_EQUAL(0, (int)block_info.more);
    LONGS_EQUAL(0, block_info.offset);

    // cleanup
    GG_CoapMessage_Destroy(request);
    GG_CoapMessage_Destroy(response);
    GG_CoapEndpoint_Destroy(endpoint);
    GG_TimerScheduler_Destroy(timer_scheduler);
}

//-----------------------------------------------------------------------
// Test that we can cancel a blockwise request from *within* a listener
//-----------------------------------------------------------------------
typedef struct {
    GG_IMPLEMENTS(GG_CoapBlockwiseResponseListener);

    GG_CoapEndpoint*     endpoint;
    GG_CoapRequestHandle request_handle[2];
    size_t               offset_to_cancel_on;
    bool                 cancel_on_error;
    bool                 on_error_called;
    bool                 cancel_called;
} CancelingListener;

static void
CancelingListener_OnResponseBlock(GG_CoapBlockwiseResponseListener* _self,
                                  GG_CoapMessageBlockInfo*          block_info,
                                  GG_CoapMessage*                   block_message)
{
    CancelingListener* self = GG_SELF(CancelingListener, GG_CoapBlockwiseResponseListener);
    GG_COMPILER_UNUSED(block_message);

    CHECK_FALSE(self->cancel_called)

    if (block_info->offset >= self->offset_to_cancel_on) {
        GG_Result result;
        if (self->request_handle[0]) {
            result = GG_CoapEndpoint_CancelBlockwiseRequest(self->endpoint, self->request_handle[0]);
            self->request_handle[0] = GG_COAP_INVALID_REQUEST_HANDLE;
            LONGS_EQUAL(result, GG_SUCCESS)
        }
        if (self->request_handle[1]) {
            result = GG_CoapEndpoint_CancelBlockwiseRequest(self->endpoint, self->request_handle[1]);
            self->request_handle[1] = GG_COAP_INVALID_REQUEST_HANDLE;
            LONGS_EQUAL(result, GG_SUCCESS)
        }
        self->cancel_called = true;
    }
}

static void
CancelingListener_OnError(GG_CoapBlockwiseResponseListener* _self,
                          GG_Result                         error,
                          const char*                       message)
{
    CancelingListener* self = GG_SELF(CancelingListener, GG_CoapBlockwiseResponseListener);
    GG_COMPILER_UNUSED(error);
    GG_COMPILER_UNUSED(message);

    self->on_error_called = true;
    if (self->cancel_on_error) {
        GG_Result result;
        if (self->request_handle[0]) {
            result = GG_CoapEndpoint_CancelBlockwiseRequest(self->endpoint, self->request_handle[0]);
            self->request_handle[0] = GG_COAP_INVALID_REQUEST_HANDLE;
            LONGS_EQUAL(result, GG_SUCCESS)
        }
        if (self->request_handle[1]) {
            result = GG_CoapEndpoint_CancelBlockwiseRequest(self->endpoint, self->request_handle[1]);
            self->request_handle[1] = GG_COAP_INVALID_REQUEST_HANDLE;
            LONGS_EQUAL(result, GG_SUCCESS)
        }
        self->cancel_called   = true;
    }
}

GG_IMPLEMENT_INTERFACE(CancelingListener, GG_CoapBlockwiseResponseListener) {
    .OnResponseBlock = CancelingListener_OnResponseBlock,
    .OnError         = CancelingListener_OnError
};

TEST(GG_COAP_BLOCKWISE, Test_BlockwiseCancelFromListener) {
    GG_Result result;

    // create two endpoints
    GG_TimerScheduler* timer_scheduler1 = NULL;
    GG_TimerScheduler_Create(&timer_scheduler1);
    GG_CoapEndpoint* endpoint1;
    GG_CoapEndpoint_Create(timer_scheduler1, NULL, NULL, &endpoint1);
    GG_TimerScheduler* timer_scheduler2 = NULL;
    GG_TimerScheduler_Create(&timer_scheduler2);
    GG_CoapEndpoint* endpoint2;
    GG_CoapEndpoint_Create(timer_scheduler2, NULL, NULL, &endpoint2);

    // connect the two endpoints with async pipes
    GG_AsyncPipe* pipe1 = NULL;
    result = GG_AsyncPipe_Create(timer_scheduler1, 1, &pipe1);
    CHECK_EQUAL(GG_SUCCESS, result);
    GG_AsyncPipe* pipe2 = NULL;
    result = GG_AsyncPipe_Create(timer_scheduler2, 1, &pipe2);
    CHECK_EQUAL(GG_SUCCESS, result);
    GG_DataSource_SetDataSink(GG_CoapEndpoint_AsDataSource(endpoint1),
                              GG_AsyncPipe_AsDataSink(pipe1));
    GG_DataSource_SetDataSink(GG_AsyncPipe_AsDataSource(pipe1),
                              GG_CoapEndpoint_AsDataSink(endpoint2));
    GG_DataSource_SetDataSink(GG_CoapEndpoint_AsDataSource(endpoint2),
                              GG_AsyncPipe_AsDataSink(pipe2));
    GG_DataSource_SetDataSink(GG_AsyncPipe_AsDataSource(pipe2),
                              GG_CoapEndpoint_AsDataSink(endpoint1));

    // create and register a handler2
    Handler2 handler2 = {
        GG_INTERFACE_INITIALIZER(Handler2, GG_CoapRequestHandler)
    };
    handler2.payload_size   = 10000;
    handler2.response_delay = 0;
    GG_CoapEndpoint_RegisterRequestHandler(endpoint2,
                                           "handler2",
                                           GG_COAP_REQUEST_HANDLER_FLAG_ALLOW_GET,
                                           GG_CAST(&handler2, GG_CoapRequestHandler));

    // create and register a handler4, setup for causing an error
    Handler4 handler4 = {
        GG_INTERFACE_INITIALIZER(Handler4, GG_CoapRequestHandler)
    };
    Handler4Item items[] = {
        {
            .payload_size  = 123,
            .response_code = GG_COAP_MESSAGE_CODE_CONTENT,
            .option        = GG_COAP_MESSAGE_OPTION_BLOCK2,
            .block_info = {
                .offset = 1024,
                .size   = 1024,
                .more   = true
            }
        }
    };
    handler4.items = items;
    handler4.item_count = GG_ARRAY_SIZE(items);
    GG_CoapEndpoint_RegisterRequestHandler(endpoint2,
                                           "handler4",
                                           GG_COAP_REQUEST_HANDLER_FLAG_ALLOW_GET,
                                           GG_CAST(&handler4, GG_CoapRequestHandler));

    // create a block source
    BlockSource block_source = {
        GG_INTERFACE_INITIALIZER(BlockSource, GG_CoapBlockSource)
    };
    block_source.payload_size = 10000;

    // create a canceling listener
    CancelingListener listener = {
        GG_INTERFACE_INITIALIZER(CancelingListener, GG_CoapBlockwiseResponseListener),
        endpoint1,
        {0, 0},
        1024,
        true
    };

    // make a first blockwise GET request for handler2
    GG_CoapMessageOptionParam params1[1] = {
        GG_COAP_MESSAGE_OPTION_PARAM_STRING(URI_PATH, "handler2")
    };
    result = GG_CoapEndpoint_SendBlockwiseRequest(endpoint1,
                                                  GG_COAP_METHOD_GET,
                                                  params1, GG_ARRAY_SIZE(params1),
                                                  NULL,
                                                  0,
                                                  NULL,
                                                  GG_CAST(&listener, GG_CoapBlockwiseResponseListener),
                                                  &listener.request_handle[0]);
    LONGS_EQUAL(GG_SUCCESS, result)

    // make a second blockwise GET request for handler2
    listener.on_error_called = false;
    listener.cancel_called = false;
    result = GG_CoapEndpoint_SendBlockwiseRequest(endpoint1,
                                                  GG_COAP_METHOD_GET,
                                                  params1, GG_ARRAY_SIZE(params1),
                                                  NULL,
                                                  0,
                                                  NULL,
                                                  GG_CAST(&listener, GG_CoapBlockwiseResponseListener),
                                                  &listener.request_handle[1]);
    LONGS_EQUAL(GG_SUCCESS, result)

    uint32_t now1 = 0;
    GG_TimerScheduler_SetTime(timer_scheduler1, ++now1);

    uint32_t now2 = 0;
    GG_TimerScheduler_SetTime(timer_scheduler2, ++now2);

    for (unsigned int i = 0; i < 10; i++) {
        GG_TimerScheduler_SetTime(timer_scheduler1, ++now1);
        GG_TimerScheduler_SetTime(timer_scheduler2, ++now2);
    }
    CHECK_TRUE(listener.cancel_called)

    // make a blockwise GET request for handler4
    GG_CoapMessageOptionParam params2[1] = {
        GG_COAP_MESSAGE_OPTION_PARAM_STRING(URI_PATH, "handler4")
    };
    listener.on_error_called = false;
    listener.cancel_called = false;
    listener.cancel_on_error = true;
    listener.request_handle[0] = GG_COAP_INVALID_REQUEST_HANDLE;
    listener.request_handle[1] = GG_COAP_INVALID_REQUEST_HANDLE;
    result = GG_CoapEndpoint_SendBlockwiseRequest(endpoint1,
                                                  GG_COAP_METHOD_GET,
                                                  params2, GG_ARRAY_SIZE(params2),
                                                  NULL,
                                                  0,
                                                  NULL,
                                                  GG_CAST(&listener, GG_CoapBlockwiseResponseListener),
                                                  &listener.request_handle[0]);
    LONGS_EQUAL(GG_SUCCESS, result)

    for (unsigned int i = 0; i < 10; i++) {
        GG_TimerScheduler_SetTime(timer_scheduler1, ++now1);
        GG_TimerScheduler_SetTime(timer_scheduler2, ++now2);
    }
    CHECK_TRUE(listener.on_error_called)
    CHECK_TRUE(listener.cancel_called)

    // cleanup
    GG_DataSource_SetDataSink(GG_CoapEndpoint_AsDataSource(endpoint1), NULL);
    GG_DataSource_SetDataSink(GG_AsyncPipe_AsDataSource(pipe1), NULL);
    GG_DataSource_SetDataSink(GG_CoapEndpoint_AsDataSource(endpoint2), NULL);
    GG_DataSource_SetDataSink(GG_AsyncPipe_AsDataSource(pipe2), NULL);
    GG_CoapEndpoint_Destroy(endpoint1);
    GG_CoapEndpoint_Destroy(endpoint2);
    GG_AsyncPipe_Destroy(pipe1);
    GG_AsyncPipe_Destroy(pipe2);
    GG_TimerScheduler_Destroy(timer_scheduler1);
    GG_TimerScheduler_Destroy(timer_scheduler2);
}

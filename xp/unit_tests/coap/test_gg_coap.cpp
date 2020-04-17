// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

#include "CppUTest/TestHarness.h"
#include "CppUTest/MemoryLeakDetectorNewMacros.h"

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <string.h>

#define GG_COAP_ENDPOINT_PRIVATE

#include "xp/common/gg_io.h"
#include "xp/common/gg_port.h"
#include "xp/common/gg_buffer.h"
#include "xp/common/gg_memory.h"
#include "xp/coap/gg_coap.h"
#include "xp/coap/gg_coap_endpoint.h"
#include "xp/coap/gg_coap_message.h"
#include "xp/coap/gg_coap_filters.h"
#include "xp/utils/gg_memory_data_sink.h"

/*----------------------------------------------------------------------
|   globals
+---------------------------------------------------------------------*/
static GG_CoapEndpoint* test_endpoint;
static GG_TimerScheduler* timer_scheduler;

//----------------------------------------------------------------------
//  Memory Data Sink
//----------------------------------------------------------------------
typedef struct {
    GG_IMPLEMENTS(GG_DataSink);

    GG_DataSinkListener* listener;
    bool                 block;
    unsigned int         blocked_count;
    unsigned int         receive_count;
    GG_Buffer*           last_received_buffer;
} MemSink;

static MemSink mem_sink;

static GG_Result
MemSink_PutData(GG_DataSink* _self, GG_Buffer* data, const GG_BufferMetadata* metadata)
{
    MemSink* self = (MemSink*)GG_SELF(MemSink, GG_DataSink);
    GG_COMPILER_UNUSED(metadata);

    if (self->block) {
        ++self->blocked_count;
        return GG_ERROR_WOULD_BLOCK;
    }

    // keep the buffer
    if (self->last_received_buffer) {
        GG_Buffer_Release(self->last_received_buffer);
    }
    self->last_received_buffer = GG_Buffer_Retain(data);
    ++self->receive_count;

    return GG_SUCCESS;
}

static GG_Result
MemSink_SetListener(GG_DataSink* _self, GG_DataSinkListener* listener)
{
    MemSink* self = (MemSink*)GG_SELF(MemSink, GG_DataSink);
    self->listener = listener;

    return GG_SUCCESS;
}

static void
MemSink_Reset(MemSink* self) {
    if (self->last_received_buffer) {
        GG_Buffer_Release(self->last_received_buffer);
    }
    self->last_received_buffer = NULL;
    self->receive_count = 0;
    self->block = false;
    self->blocked_count = 0;
}

GG_IMPLEMENT_INTERFACE(MemSink, GG_DataSink) {
    MemSink_PutData,
    MemSink_SetListener
};

//----------------------------------------------------------------------
//  Error Data Sink
//----------------------------------------------------------------------
typedef struct {
    GG_IMPLEMENTS(GG_DataSink);
} ErrorSink;

static ErrorSink error_sink;

static GG_Result
ErrorSink_PutData(GG_DataSink* _self, GG_Buffer* data, const GG_BufferMetadata* metadata)
{
    GG_COMPILER_UNUSED(_self);
    GG_COMPILER_UNUSED(data);
    GG_COMPILER_UNUSED(metadata);

    return GG_FAILURE;
}

static GG_Result
ErrorSink_SetListener(GG_DataSink* _self, GG_DataSinkListener* listener)
{
    GG_COMPILER_UNUSED(_self);
    GG_COMPILER_UNUSED(listener);

    return GG_SUCCESS;
}

GG_IMPLEMENT_INTERFACE(ErrorSink, GG_DataSink) {
    ErrorSink_PutData,
    ErrorSink_SetListener
};

//----------------------------------------------------------------------
//  Null Data Source
//----------------------------------------------------------------------
typedef struct {
    GG_IMPLEMENTS(GG_DataSource);

    GG_DataSink* sink;
} NullSource;

static NullSource null_source;

static GG_Result
NullSource_SetDataSink(GG_DataSource* _self, GG_DataSink* sink)
{
    NullSource* self = (NullSource*)GG_SELF(NullSource, GG_DataSource);

    self->sink = sink;
    return GG_SUCCESS;
}

GG_IMPLEMENT_INTERFACE(NullSource, GG_DataSource) {
    NullSource_SetDataSink
};

//----------------------------------------------------------------------
//  Test Client
//----------------------------------------------------------------------
typedef struct {
    GG_IMPLEMENTS(GG_CoapResponseListener);

    GG_CoapEndpoint*     endpoint;
    GG_CoapRequestHandle request_handle;
    bool                 cancel_request_on_response;
    bool                 ack_received;
    GG_Result            last_error_received;
    GG_CoapMessage*      response;
} TestClient;

static void
TestClient_OnAck(GG_CoapResponseListener* _self)
{
    TestClient* self = GG_SELF(TestClient, GG_CoapResponseListener);

    self->ack_received = true;
}

static void
TestClient_OnError(GG_CoapResponseListener* _self, GG_Result error, const char* message)
{
    TestClient* self = GG_SELF(TestClient, GG_CoapResponseListener);
    GG_COMPILER_UNUSED(message);

    self->last_error_received = error;
}

static void
TestClient_OnResponse(GG_CoapResponseListener* _self, GG_CoapMessage* response)
{
    TestClient* self = GG_SELF(TestClient, GG_CoapResponseListener);

    // free any previous response
    if (self->response) {
        GG_CoapMessage_Destroy(self->response);
        self->response = NULL;
    }

    // clone the response
    GG_Buffer* datagram = NULL;
    GG_Result result = GG_CoapMessage_ToDatagram(response, &datagram);
    CHECK_EQUAL(GG_SUCCESS, result);
    result = GG_CoapMessage_CreateFromDatagram(datagram, &self->response);
    GG_Buffer_Release(datagram);
    CHECK_EQUAL(GG_SUCCESS, result);
    CHECK_TRUE(self->response != NULL);

    if (self->cancel_request_on_response && self->endpoint) {
        GG_CoapEndpoint_CancelRequest(self->endpoint, self->request_handle);
    }
}

GG_IMPLEMENT_INTERFACE(TestClient, GG_CoapResponseListener) {
    .OnAck      = TestClient_OnAck,
    .OnError    = TestClient_OnError,
    .OnResponse = TestClient_OnResponse
};

static void
TestClient_Init(TestClient* self)
{
    memset(self, 0, sizeof(TestClient));
    GG_SET_INTERFACE(self, TestClient, GG_CoapResponseListener);
}

static void
TestClient_Cleanup(TestClient* self)
{
    if (self->response) {
        GG_CoapMessage_Destroy(self->response);
        self->response = NULL;
    }
}

/*----------------------------------------------------------------------
|   tests
+---------------------------------------------------------------------*/
TEST_GROUP(GG_COAP) {
    void setup(void) {
        GG_TimerScheduler_Create(&timer_scheduler);
        GG_SET_INTERFACE(&mem_sink, MemSink, GG_DataSink);
        GG_SET_INTERFACE(&null_source, NullSource, GG_DataSource);
        GG_SET_INTERFACE(&error_sink, ErrorSink, GG_DataSink);
        GG_CoapEndpoint_Create(timer_scheduler,
                               GG_CAST(&mem_sink, GG_DataSink),
                               GG_CAST(&null_source, GG_DataSource),
                               &test_endpoint);
    }

    void teardown(void) {
        GG_CoapEndpoint_Destroy(test_endpoint);
        GG_TimerScheduler_Destroy(timer_scheduler);
    }
};

TEST(GG_COAP, Test_CancelRequest) {
    GG_TimerScheduler_SetTime(timer_scheduler, 0);

    GG_CoapRequestHandle request_handle = 0;
    GG_Result result = GG_CoapEndpoint_SendRequest(test_endpoint, GG_COAP_METHOD_GET,
                                                   NULL, 0, NULL, 0, NULL, NULL, &request_handle);
    CHECK_EQUAL(GG_SUCCESS, result);

    GG_CoapRequestHandle bogus_handle = 123456789;
    result = GG_CoapEndpoint_CancelRequest(test_endpoint, bogus_handle);
    CHECK_EQUAL(GG_ERROR_NO_SUCH_ITEM, result);

    result = GG_CoapEndpoint_CancelRequest(test_endpoint, request_handle);
    CHECK_EQUAL(GG_SUCCESS, result);

    result = GG_CoapEndpoint_CancelRequest(test_endpoint, request_handle);
    CHECK_EQUAL(GG_ERROR_NO_SUCH_ITEM, result);

    GG_TimerScheduler_SetTime(timer_scheduler, 100);
}

TEST(GG_COAP, Test_CancelFromWithinListener) {
    GG_TimerScheduler_SetTime(timer_scheduler, 0);

    TestClient test_client;
    TestClient_Init(&test_client);
    GG_CoapMessage* message = NULL;

    // setup the client
    test_client.endpoint = test_endpoint;
    test_client.cancel_request_on_response = true;

    // send a request
    GG_Result result = GG_CoapEndpoint_SendRequest(test_endpoint,
                                                   GG_COAP_METHOD_GET,
                                                   NULL,
                                                   0,
                                                   NULL,
                                                   0,
                                                   NULL,
                                                   GG_CAST(&test_client, GG_CoapResponseListener),
                                                   &test_client.request_handle);
    CHECK_EQUAL(GG_SUCCESS, result);
    CHECK_TRUE(mem_sink.last_received_buffer != NULL);
    result = GG_CoapMessage_CreateFromDatagram(mem_sink.last_received_buffer, &message);
    CHECK_EQUAL(GG_SUCCESS, result);
    CHECK_TRUE(message != NULL);
    uint8_t token[GG_COAP_MESSGAGE_MAX_TOKEN_LENGTH];
    size_t  token_length = GG_CoapMessage_GetToken(message, token);
    CHECK_TRUE(token_length <= GG_COAP_MESSGAGE_MAX_TOKEN_LENGTH);

    // create a response
    GG_CoapMessage* response = NULL;
    result = GG_CoapEndpoint_CreateResponse(test_endpoint,
                                            message,
                                            GG_COAP_MESSAGE_CODE_CONTENT,
                                            NULL,
                                            0,
                                            NULL,
                                            0,
                                            &response);
    CHECK_EQUAL(GG_SUCCESS, result);

    // send the response
    GG_Buffer* response_datagram = NULL;
    result = GG_CoapMessage_ToDatagram(response, &response_datagram);
    CHECK_EQUAL(GG_SUCCESS, result);
    GG_CoapMessage_Destroy(response);
    CHECK_FALSE(null_source.sink == NULL);
    result = GG_DataSink_PutData(null_source.sink, response_datagram, NULL);
    CHECK_EQUAL(GG_SUCCESS, result);
    GG_Buffer_Release(response_datagram);

    // check that the request is no longer there
    result = GG_CoapEndpoint_CancelRequest(test_endpoint, test_client.request_handle);
    CHECK_EQUAL(GG_ERROR_NO_SUCH_ITEM, result);

    TestClient_Cleanup(&test_client);
    GG_CoapMessage_Destroy(message);

    GG_TimerScheduler_SetTime(timer_scheduler, 100);
}

TEST(GG_COAP, Test_RequestToken) {
    MemSink_Reset(&mem_sink);
    GG_CoapRequestHandle request_handle = 0;
    GG_CoapMessage* message = NULL;

    // send a first message
    GG_Result result = GG_CoapEndpoint_SendRequest(test_endpoint, GG_COAP_METHOD_GET,
                                                   NULL, 0, NULL, 0, NULL, NULL, &request_handle);
    CHECK_EQUAL(GG_SUCCESS, result);
    CHECK_TRUE(mem_sink.last_received_buffer != NULL);
    result = GG_CoapMessage_CreateFromDatagram(mem_sink.last_received_buffer, &message);
    CHECK_EQUAL(GG_SUCCESS, result);
    CHECK_TRUE(message != NULL);
    uint8_t token1[GG_COAP_MESSGAGE_MAX_TOKEN_LENGTH];
    size_t  token_length1 = GG_CoapMessage_GetToken(message, token1);
    CHECK_TRUE(token_length1 <= GG_COAP_MESSGAGE_MAX_TOKEN_LENGTH);
    GG_CoapMessage_Destroy(message);

    // send a second message
    result = GG_CoapEndpoint_SendRequest(test_endpoint, GG_COAP_METHOD_GET,
                                         NULL, 0, NULL, 0, NULL, NULL, &request_handle);
    CHECK_EQUAL(GG_SUCCESS, result);
    CHECK_TRUE(mem_sink.last_received_buffer != NULL);
    result = GG_CoapMessage_CreateFromDatagram(mem_sink.last_received_buffer, &message);
    CHECK_EQUAL(GG_SUCCESS, result);
    CHECK_TRUE(message != NULL);

    // check that the second token is different from the first
    uint8_t token2[GG_COAP_MESSGAGE_MAX_TOKEN_LENGTH];
    size_t  token_length2 = GG_CoapMessage_GetToken(message, token2);
    if (token_length1 == token_length2) {
        CHECK_FALSE(memcmp(token1, token2, token_length1) == 0);
    }

    // cleanup
    GG_CoapMessage_Destroy(message);
    MemSink_Reset(&mem_sink);
}

static uint32_t
CalculateRetryAbsoluteTime(uint32_t retry_count) {
    GG_ASSERT(retry_count >= 1);
    return retry_count == 1
        ? (uint32_t)(GG_COAP_ACK_TIMEOUT_MS * GG_COAP_ACK_RANDOM_FACTOR)
        : CalculateRetryAbsoluteTime(retry_count - 1) * 3;
}

TEST(GG_COAP, Test_Resend) {
    TestClient test_client;
    TestClient_Init(&test_client);
    GG_Result result;

    MemSink_Reset(&mem_sink);
    GG_TimerScheduler_SetTime(timer_scheduler, 0);

    // make the sink block
    mem_sink.block = true;

    // send a request
    result = GG_CoapEndpoint_SendRequest(test_endpoint,
                                         GG_COAP_METHOD_GET,
                                         NULL,
                                         0,
                                         NULL,
                                         0,
                                         NULL,
                                         GG_CAST(&test_client, GG_CoapResponseListener),
                                         &test_client.request_handle);
    LONGS_EQUAL(GG_SUCCESS, result);

    // check that a delivery attempt has been made and blocked
    LONGS_EQUAL(1, mem_sink.blocked_count);

    // unblock the sink
    mem_sink.block = false;

    // notify that we're Ok to receive
    CHECK_TRUE(mem_sink.listener != NULL);
    GG_DataSinkListener_OnCanPut(mem_sink.listener);

    // check that the datagram was delivered
    LONGS_EQUAL(1, mem_sink.blocked_count);
    LONGS_EQUAL(1, mem_sink.receive_count);
    CHECK_TRUE(mem_sink.last_received_buffer != NULL);

    // advance the timer to half the retry timeout and check that nothing has been resent yet
    GG_TimerScheduler_SetTime(timer_scheduler, CalculateRetryAbsoluteTime(1) >> 1);
    LONGS_EQUAL(1, mem_sink.blocked_count);
    LONGS_EQUAL(1, mem_sink.receive_count);

    // advance the timer past the 1rst retry timeout and check that one resend has occurred
    GG_TimerScheduler_SetTime(timer_scheduler, CalculateRetryAbsoluteTime(1));
    LONGS_EQUAL(1, mem_sink.blocked_count);
    LONGS_EQUAL(2, mem_sink.receive_count);

    // make the sink block
    mem_sink.block = true;

    // advance the timer past the 2nd retry and check that one resend has occurred and was blocked
    GG_TimerScheduler_SetTime(timer_scheduler, CalculateRetryAbsoluteTime(2));
    LONGS_EQUAL(2, mem_sink.blocked_count);
    LONGS_EQUAL(2, mem_sink.receive_count);

    // advance the past the 3rd retry and check that one resend has occurred and was blocked
    GG_TimerScheduler_SetTime(timer_scheduler, CalculateRetryAbsoluteTime(3));
    LONGS_EQUAL(3, mem_sink.blocked_count);
    LONGS_EQUAL(2, mem_sink.receive_count);

    // unblock
    mem_sink.block = false;
    GG_DataSinkListener_OnCanPut(mem_sink.listener);

    // check that the datagram was delivered
    LONGS_EQUAL(3, mem_sink.blocked_count);
    LONGS_EQUAL(3, mem_sink.receive_count);
    CHECK_TRUE(mem_sink.last_received_buffer != NULL);

    // advance the timer past the 4th retry and check that the last resend has been done
    GG_TimerScheduler_SetTime(timer_scheduler, CalculateRetryAbsoluteTime(4));
    LONGS_EQUAL(3, mem_sink.blocked_count);
    LONGS_EQUAL(4, mem_sink.receive_count);
    LONGS_EQUAL(GG_SUCCESS, test_client.last_error_received);

    // advance the timer past the 5th retry and check that the entire request has timed out
    GG_TimerScheduler_SetTime(timer_scheduler, CalculateRetryAbsoluteTime(5));
    LONGS_EQUAL(3, mem_sink.blocked_count);
    LONGS_EQUAL(4, mem_sink.receive_count);
    LONGS_EQUAL(GG_ERROR_TIMEOUT, test_client.last_error_received);
}

TEST(GG_COAP, Test_ResendBounds) {
    TestClient test_client;
    TestClient_Init(&test_client);
    GG_Result result;

    // make the sink block
    MemSink_Reset(&mem_sink);
    mem_sink.block = true;

    // reset the clock and the client
    GG_TimerScheduler_SetTime(timer_scheduler, 0);
    TestClient_Init(&test_client);

    // send a request
    result = GG_CoapEndpoint_SendRequest(test_endpoint,
                                         GG_COAP_METHOD_GET,
                                         NULL,
                                         0,
                                         NULL,
                                         0,
                                         NULL,
                                         GG_CAST(&test_client, GG_CoapResponseListener),
                                         &test_client.request_handle);
    LONGS_EQUAL(GG_SUCCESS, result);

    // move the time forward by increments of 100ms until there's a timeout error received
    for (size_t t = 0; t < 500000; t += 100) {
        GG_TimerScheduler_SetTime(timer_scheduler, (uint32_t)t);
        if (test_client.last_error_received == GG_ERROR_TIMEOUT) {
            // check that the total time elapsed is within bounds
            uint32_t lower_bound =
                (uint32_t)((1 + 2 + 4 + 8) * GG_COAP_ACK_TIMEOUT_MS);
            uint32_t upper_bound =
                (uint32_t)((1 + 2 + 4 + 8 + 16) * GG_COAP_ACK_TIMEOUT_MS * GG_COAP_ACK_RANDOM_FACTOR);
            CHECK_TRUE(t >= lower_bound && t <= upper_bound);
            break;
        }
    }

    LONGS_EQUAL(test_client.last_error_received, GG_ERROR_TIMEOUT);
}

TEST(GG_COAP, Test_CustomClientTimeout) {
    TestClient test_client;
    TestClient_Init(&test_client);
    GG_Result result;

    // make the sink block
    MemSink_Reset(&mem_sink);
    mem_sink.block = true;

    // reset the clock and the client
    GG_TimerScheduler_SetTime(timer_scheduler, 0);
    TestClient_Init(&test_client);

    // send a request
    GG_CoapClientParameters client_parameters = {
        .ack_timeout      = 100,
        .max_resend_count = 3
    };
    result = GG_CoapEndpoint_SendRequest(test_endpoint,
                                         GG_COAP_METHOD_GET,
                                         NULL,
                                         0,
                                         NULL,
                                         0,
                                         &client_parameters,
                                         GG_CAST(&test_client, GG_CoapResponseListener),
                                         &test_client.request_handle);
    LONGS_EQUAL(GG_SUCCESS, result);

    // move the time forward by increments of 10ms until there's a timeout error received
    for (size_t t = 0; t < 50000; t += 10) {
        GG_TimerScheduler_SetTime(timer_scheduler, (uint32_t)t);
        if (test_client.last_error_received == GG_ERROR_TIMEOUT) {
            // check that the total time elapsed is within bounds
            uint32_t expected_timeout = (1 + 2 + 4 + 8) * client_parameters.ack_timeout;
            CHECK_TRUE(t >= expected_timeout - 10 && t <= expected_timeout + 10);
            break;
        }
    }

    LONGS_EQUAL(test_client.last_error_received, GG_ERROR_TIMEOUT);
}

TEST(GG_COAP, Test_CustomClientResendCount) {
    TestClient test_client;
    TestClient_Init(&test_client);
    GG_Result result;

    // make the sink block
    MemSink_Reset(&mem_sink);
    mem_sink.block = true;

    // reset the clock and the client
    GG_TimerScheduler_SetTime(timer_scheduler, 0);
    TestClient_Init(&test_client);

    // send a request
    GG_CoapClientParameters client_parameters = {
        .ack_timeout      = 1000,
        .max_resend_count = 0
    };
    result = GG_CoapEndpoint_SendRequest(test_endpoint,
                                         GG_COAP_METHOD_GET,
                                         NULL,
                                         0,
                                         NULL,
                                         0,
                                         &client_parameters,
                                         GG_CAST(&test_client, GG_CoapResponseListener),
                                         &test_client.request_handle);
    LONGS_EQUAL(GG_SUCCESS, result);

    // move the time forward by increments of 10ms until there's a timeout error received
    for (size_t t = 0; t < 50000; t += 10) {
        GG_TimerScheduler_SetTime(timer_scheduler, (uint32_t)t);
        if (test_client.last_error_received == GG_ERROR_TIMEOUT) {
            // check that the total time elapsed is within bounds
            uint32_t expected_timeout = 1000;
            CHECK_TRUE(t >= expected_timeout - 10 && t <= expected_timeout + 10);
            break;
        }
    }

    LONGS_EQUAL(test_client.last_error_received, GG_ERROR_TIMEOUT);
}

TEST(GG_COAP, Test_TransportError) {
    TestClient test_client;
    TestClient_Init(&test_client);
    GG_Result result;

    GG_TimerScheduler_SetTime(timer_scheduler, 0);

    // use the error sink
    GG_DataSource_SetDataSink(GG_CoapEndpoint_AsDataSource(test_endpoint), GG_CAST(&error_sink, GG_DataSink));

    // send a request
    result = GG_CoapEndpoint_SendRequest(test_endpoint,
                                         GG_COAP_METHOD_GET,
                                         NULL,
                                         0,
                                         NULL,
                                         0,
                                         NULL,
                                         GG_CAST(&test_client, GG_CoapResponseListener),
                                         &test_client.request_handle);
    LONGS_EQUAL(GG_SUCCESS, result);
    LONGS_EQUAL(GG_ERROR_COAP_SEND_FAILURE, test_client.last_error_received);

    GG_TimerScheduler_SetTime(timer_scheduler, 100);
}

TEST(GG_COAP, Test_ConnectionChange) {
    TestClient test_client;
    TestClient_Init(&test_client);
    GG_Result result;

    MemSink_Reset(&mem_sink);
    GG_TimerScheduler_SetTime(timer_scheduler, 0);

    // NULL the endpoint's sink
    GG_DataSource_SetDataSink(GG_CoapEndpoint_AsDataSource(test_endpoint), NULL);

    // send a request
    result = GG_CoapEndpoint_SendRequest(test_endpoint,
                                         GG_COAP_METHOD_GET,
                                         NULL,
                                         0,
                                         NULL,
                                         0,
                                         NULL,
                                         GG_CAST(&test_client, GG_CoapResponseListener),
                                         &test_client.request_handle);
    LONGS_EQUAL(GG_SUCCESS, result);

    // check that nothing has been delivered
    LONGS_EQUAL(0, mem_sink.receive_count);

    // set a connection sink
    GG_DataSource_SetDataSink(GG_CoapEndpoint_AsDataSource(test_endpoint), GG_CAST(&mem_sink, GG_DataSink));

    // check that the request has now been delivered
    LONGS_EQUAL(1, mem_sink.receive_count);
}

typedef struct {
    GG_IMPLEMENTS(GG_CoapRequestHandler);

    bool      was_called;
    uint8_t   last_message_code_handled;
    uint8_t   code_to_respond_with;
    GG_Result result_to_return;
} TestHandler;

static GG_Result
TestHandler_OnRequest(GG_CoapRequestHandler*   _self,
                      GG_CoapEndpoint*         endpoint,
                      const GG_CoapMessage*    request,
                      GG_CoapResponder*        responder,
                      const GG_BufferMetadata* transport_metadata,
                      GG_CoapMessage**         response)
{
    TestHandler* self = GG_SELF(TestHandler, GG_CoapRequestHandler);
    GG_COMPILER_UNUSED(responder);
    GG_COMPILER_UNUSED(transport_metadata);

    self->was_called = true;
    self->last_message_code_handled = GG_CoapMessage_GetCode(request);

    if (self->code_to_respond_with != 0) {
        GG_Result result =  GG_CoapEndpoint_CreateResponse(endpoint,
                                                           request,
                                                           self->code_to_respond_with,
                                                           NULL, 0, NULL, 0,
                                                           response);
        if (GG_FAILED(result)) {
            return result;
        }
    } else {
        *response = NULL;
    }
    return self->result_to_return;
}

GG_IMPLEMENT_INTERFACE(TestHandler, GG_CoapRequestHandler) {
    .OnRequest = TestHandler_OnRequest
};

TEST(GG_COAP, Test_Handlers) {
    // create two endpoints and connect them together
    GG_TimerScheduler* timer_scheduler1 = NULL;
    GG_TimerScheduler_Create(&timer_scheduler1);
    GG_CoapEndpoint* endpoint1;
    GG_CoapEndpoint_Create(timer_scheduler1, NULL, NULL, &endpoint1);
    GG_CoapEndpoint* endpoint2;
    GG_CoapEndpoint_Create(timer_scheduler1, NULL, NULL, &endpoint2);
    GG_DataSource_SetDataSink(GG_CoapEndpoint_AsDataSource(endpoint1),
                              GG_CoapEndpoint_AsDataSink(endpoint2));
    GG_DataSource_SetDataSink(GG_CoapEndpoint_AsDataSource(endpoint2),
                              GG_CoapEndpoint_AsDataSink(endpoint1));

    // register a test handler
    TestHandler handler1;
    GG_SET_INTERFACE(&handler1, TestHandler, GG_CoapRequestHandler);
    handler1.result_to_return = GG_COAP_MESSAGE_CODE_CREATED;
    handler1.code_to_respond_with = 0;
    handler1.last_message_code_handled = 0;
    handler1.was_called = false;
    GG_Result result = GG_CoapEndpoint_RegisterRequestHandler(endpoint2,
                                                              "foo/bar/1",
                                                              GG_COAP_REQUEST_HANDLER_FLAG_ALLOW_GET,
                                                              GG_CAST(&handler1, GG_CoapRequestHandler));
    LONGS_EQUAL(GG_SUCCESS, result);

    // init a test client
    TestClient client1;
    TestClient_Init(&client1);

    // send a GET request to an invalid path
    GG_CoapMessageOptionParam options[] = {
        GG_COAP_MESSAGE_OPTION_PARAM_STRING(URI_PATH, "bla")
    };
    result = GG_CoapEndpoint_SendRequest(endpoint1,
                                         GG_COAP_METHOD_POST,
                                         options, GG_ARRAY_SIZE(options),
                                         NULL, 0,
                                         NULL,
                                         GG_CAST(&client1, GG_CoapResponseListener),
                                         &client1.request_handle);
    LONGS_EQUAL(GG_SUCCESS, result);

    // check that the handler was not called
    CHECK_FALSE(handler1.was_called);

    // check that we got "not found" error
    CHECK_TRUE(client1.ack_received);
    CHECK_TRUE(client1.response != NULL);
    LONGS_EQUAL(GG_COAP_MESSAGE_CODE_NOT_FOUND, GG_CoapMessage_GetCode(client1.response));

    TestClient_Cleanup(&client1);
    GG_CoapMessageOptionParam options1[] = {
        GG_COAP_MESSAGE_OPTION_PARAM_STRING(URI_PATH, "foo"),
        GG_COAP_MESSAGE_OPTION_PARAM_STRING(URI_PATH, "bar"),
        GG_COAP_MESSAGE_OPTION_PARAM_STRING(URI_PATH, "1")
    };

    // send a POST request (should be filtered out)
    result = GG_CoapEndpoint_SendRequest(endpoint1,
                                         GG_COAP_METHOD_POST,
                                         options1, GG_ARRAY_SIZE(options1),
                                         NULL, 0,
                                         NULL,
                                         GG_CAST(&client1, GG_CoapResponseListener),
                                         &client1.request_handle);
    LONGS_EQUAL(GG_SUCCESS, result);

    // check that the handler was not called
    CHECK_FALSE(handler1.was_called);

    // check that we got an "invalid method" error
    CHECK_TRUE(client1.ack_received);
    CHECK_TRUE(client1.response != NULL);
    LONGS_EQUAL(GG_COAP_MESSAGE_CODE_METHOD_NOT_ALLOWED, GG_CoapMessage_GetCode(client1.response));

    // send a PUT request (should be filtered out)
    result = GG_CoapEndpoint_SendRequest(endpoint1,
                                         GG_COAP_METHOD_PUT,
                                         options1, GG_ARRAY_SIZE(options1),
                                         NULL, 0,
                                         NULL,
                                         GG_CAST(&client1, GG_CoapResponseListener),
                                         &client1.request_handle);
    LONGS_EQUAL(GG_SUCCESS, result);

    // check that the handler was not called
    CHECK_FALSE(handler1.was_called);

    // check that we got an "invalid method" error
    CHECK_TRUE(client1.ack_received);
    CHECK_TRUE(client1.response != NULL);
    LONGS_EQUAL(GG_COAP_MESSAGE_CODE_METHOD_NOT_ALLOWED, GG_CoapMessage_GetCode(client1.response));

    // send a DELETE request (should be filtered out)
    result = GG_CoapEndpoint_SendRequest(endpoint1,
                                         GG_COAP_METHOD_DELETE,
                                         options1, GG_ARRAY_SIZE(options1),
                                         NULL, 0,
                                         NULL,
                                         GG_CAST(&client1, GG_CoapResponseListener),
                                         &client1.request_handle);
    LONGS_EQUAL(GG_SUCCESS, result);

    // check that the handler was not called
    CHECK_FALSE(handler1.was_called);

    // check that we got an "invalid method" error
    CHECK_TRUE(client1.ack_received);
    CHECK_TRUE(client1.response != NULL);
    LONGS_EQUAL(GG_COAP_MESSAGE_CODE_METHOD_NOT_ALLOWED, GG_CoapMessage_GetCode(client1.response));

    // send a GET request (should not be filtered out)
    TestClient_Cleanup(&client1);
    result = GG_CoapEndpoint_SendRequest(endpoint1,
                                         GG_COAP_METHOD_GET,
                                         options1, GG_ARRAY_SIZE(options1),
                                         NULL, 0,
                                         NULL,
                                         GG_CAST(&client1, GG_CoapResponseListener),
                                         &client1.request_handle);
    LONGS_EQUAL(GG_SUCCESS, result);

    // check that the handler was called
    CHECK_TRUE(handler1.was_called);

    // check that we got valid response
    CHECK_TRUE(client1.ack_received);
    CHECK_TRUE(client1.response != NULL);
    LONGS_EQUAL(GG_COAP_MESSAGE_CODE_CREATED, GG_CoapMessage_GetCode(client1.response));

    // change what the handler should return
    handler1.result_to_return = GG_SUCCESS;
    handler1.code_to_respond_with = GG_COAP_MESSAGE_CODE_CONTENT;

    // send a GET request (should not be filtered out)
    TestClient_Cleanup(&client1);
    result = GG_CoapEndpoint_SendRequest(endpoint1,
                                         GG_COAP_METHOD_GET,
                                         options1, GG_ARRAY_SIZE(options1),
                                         NULL, 0,
                                         NULL,
                                         GG_CAST(&client1, GG_CoapResponseListener),
                                         &client1.request_handle);
    LONGS_EQUAL(GG_SUCCESS, result);

    // check that the handler was called
    CHECK_TRUE(handler1.was_called);

    // check that we got valid response
    CHECK_TRUE(client1.ack_received);
    CHECK_TRUE(client1.response != NULL);
    LONGS_EQUAL(GG_COAP_MESSAGE_CODE_CONTENT, GG_CoapMessage_GetCode(client1.response));

    // change what the handler should return
    handler1.result_to_return = GG_ERROR_INVALID_FORMAT;
    handler1.code_to_respond_with = 0;

    // send a GET request (should not be filtered out)
    TestClient_Cleanup(&client1);
    result = GG_CoapEndpoint_SendRequest(endpoint1,
                                         GG_COAP_METHOD_GET,
                                         options1, GG_ARRAY_SIZE(options1),
                                         NULL, 0,
                                         NULL,
                                         GG_CAST(&client1, GG_CoapResponseListener),
                                         &client1.request_handle);
    LONGS_EQUAL(GG_SUCCESS, result);

    // check that the handler was called
    CHECK_TRUE(handler1.was_called);

    // check that we got valid response
    CHECK_TRUE(client1.ack_received);
    CHECK_TRUE(client1.response != NULL);
    LONGS_EQUAL(GG_COAP_MESSAGE_CODE_INTERNAL_SERVER_ERROR, GG_CoapMessage_GetCode(client1.response));

    // unregister the handler
    result = GG_CoapEndpoint_UnregisterRequestHandler(endpoint2, NULL, GG_CAST(&handler1, GG_CoapRequestHandler));
    LONGS_EQUAL(GG_SUCCESS, result);

    // send a GET request (should not be filtered out)
    TestClient_Cleanup(&client1);
    result = GG_CoapEndpoint_SendRequest(endpoint1,
                                         GG_COAP_METHOD_GET,
                                         options1, GG_ARRAY_SIZE(options1),
                                         NULL, 0,
                                         NULL,
                                         GG_CAST(&client1, GG_CoapResponseListener),
                                         &client1.request_handle);
    LONGS_EQUAL(GG_SUCCESS, result);

    // check that the handler was called
    CHECK_TRUE(handler1.was_called);

    // check that we got "not found" response
    CHECK_TRUE(client1.ack_received);
    CHECK_TRUE(client1.response != NULL);
    LONGS_EQUAL(GG_COAP_MESSAGE_CODE_NOT_FOUND, GG_CoapMessage_GetCode(client1.response));

    // cleanup
    TestClient_Cleanup(&client1);
    GG_TimerScheduler_Destroy(timer_scheduler1);
    GG_DataSource_SetDataSink(GG_CoapEndpoint_AsDataSource(endpoint1), NULL);
    GG_DataSource_SetDataSink(GG_CoapEndpoint_AsDataSource(endpoint2), NULL);
    GG_CoapEndpoint_Destroy(endpoint1);
    GG_CoapEndpoint_Destroy(endpoint2);
}

TEST(GG_COAP, Test_Handlers2) {
    // create two endpoints and connect them together
    GG_TimerScheduler* timer_scheduler1 = NULL;
    GG_TimerScheduler_Create(&timer_scheduler1);
    GG_CoapEndpoint* endpoint1;
    GG_CoapEndpoint_Create(timer_scheduler1, NULL, NULL, &endpoint1);
    GG_CoapEndpoint* endpoint2;
    GG_CoapEndpoint_Create(timer_scheduler1, NULL, NULL, &endpoint2);
    GG_DataSource_SetDataSink(GG_CoapEndpoint_AsDataSource(endpoint1),
                              GG_CoapEndpoint_AsDataSink(endpoint2));
    GG_DataSource_SetDataSink(GG_CoapEndpoint_AsDataSource(endpoint2),
                              GG_CoapEndpoint_AsDataSink(endpoint1));

    // register a first test handler
    TestHandler handler1;
    GG_SET_INTERFACE(&handler1, TestHandler, GG_CoapRequestHandler);
    handler1.result_to_return = GG_COAP_MESSAGE_CODE_CREATED;
    handler1.code_to_respond_with = 0;
    handler1.last_message_code_handled = 0;
    handler1.was_called = false;
    GG_Result result = GG_CoapEndpoint_RegisterRequestHandler(endpoint2,
                                                              "foo/bar",
                                                              GG_COAP_REQUEST_HANDLER_FLAG_ALLOW_GET,
                                                              GG_CAST(&handler1, GG_CoapRequestHandler));
    LONGS_EQUAL(GG_SUCCESS, result);

    // register a second test handler (with some extra "/" at the start to check that they are ignored
    TestHandler handler2;
    GG_SET_INTERFACE(&handler2, TestHandler, GG_CoapRequestHandler);
    handler2.result_to_return = GG_COAP_MESSAGE_CODE_CREATED;
    handler2.code_to_respond_with = 0;
    handler2.last_message_code_handled = 0;
    handler2.was_called = false;
    result = GG_CoapEndpoint_RegisterRequestHandler(endpoint2,
                                                    "//foo",
                                                    GG_COAP_REQUEST_HANDLER_FLAG_ALLOW_GET,
                                                    GG_CAST(&handler2, GG_CoapRequestHandler));
    LONGS_EQUAL(GG_SUCCESS, result);

    // init a test client
    TestClient client1;
    TestClient_Init(&client1);

    // send a GET request to "foo"
    GG_CoapMessageOptionParam options[4] = {
        GG_COAP_MESSAGE_OPTION_PARAM_STRING(URI_PATH, "foo"),
        GG_COAP_MESSAGE_OPTION_PARAM_STRING(URI_PATH, "bar"),
        GG_COAP_MESSAGE_OPTION_PARAM_STRING(URI_PATH, "baz"),
        GG_COAP_MESSAGE_OPTION_PARAM_STRING(URI_PATH, "fo"),
    };
    result = GG_CoapEndpoint_SendRequest(endpoint1,
                                         GG_COAP_METHOD_GET,
                                         options, 1,
                                         NULL, 0,
                                         NULL,
                                         GG_CAST(&client1, GG_CoapResponseListener),
                                         &client1.request_handle);
    LONGS_EQUAL(GG_SUCCESS, result);

    // check that handler1 was called but not handler2
    CHECK_FALSE(handler1.was_called);
    CHECK_TRUE(handler2.was_called);

    // reset
    handler1.was_called = false;
    handler2.was_called = false;

    // send a GET request to "foo/bar"
    result = GG_CoapEndpoint_SendRequest(endpoint1,
                                         GG_COAP_METHOD_GET,
                                         options, 2,
                                         NULL, 0,
                                         NULL,
                                         GG_CAST(&client1, GG_CoapResponseListener),
                                         &client1.request_handle);
    LONGS_EQUAL(GG_SUCCESS, result);

    // check that handler1 was called but not handler2
    CHECK_TRUE(handler1.was_called);
    CHECK_FALSE(handler2.was_called);

    // reset
    handler1.was_called = false;
    handler2.was_called = false;

    // send a GET request to "foo/bar/baz"
    result = GG_CoapEndpoint_SendRequest(endpoint1,
                                         GG_COAP_METHOD_GET,
                                         options, 3,
                                         NULL, 0,
                                         NULL,
                                         GG_CAST(&client1, GG_CoapResponseListener),
                                         &client1.request_handle);
    LONGS_EQUAL(GG_SUCCESS, result);

    // check that handler1 was called but not handler2
    CHECK_TRUE(handler1.was_called);
    CHECK_FALSE(handler2.was_called);

    // reset
    handler1.was_called = false;
    handler2.was_called = false;

    // send a GET request to "bar"
    result = GG_CoapEndpoint_SendRequest(endpoint1,
                                         GG_COAP_METHOD_GET,
                                         &options[1], 1,
                                         NULL, 0,
                                         NULL,
                                         GG_CAST(&client1, GG_CoapResponseListener),
                                         &client1.request_handle);
    LONGS_EQUAL(GG_SUCCESS, result);

    // check that no handler was called
    CHECK_FALSE(handler1.was_called);
    CHECK_FALSE(handler2.was_called);

    // reset
    handler1.was_called = false;
    handler2.was_called = false;

    // send a GET request to "fo"
    result = GG_CoapEndpoint_SendRequest(endpoint1,
                                         GG_COAP_METHOD_GET,
                                         &options[3], 1,
                                         NULL, 0,
                                         NULL,
                                         GG_CAST(&client1, GG_CoapResponseListener),
                                         &client1.request_handle);
    LONGS_EQUAL(GG_SUCCESS, result);

    // check that no handler was called
    CHECK_FALSE(handler1.was_called);
    CHECK_FALSE(handler2.was_called);

    // reset
    handler1.was_called = false;
    handler2.was_called = false;

    // cleanup
    TestClient_Cleanup(&client1);
    GG_TimerScheduler_Destroy(timer_scheduler1);
    GG_DataSource_SetDataSink(GG_CoapEndpoint_AsDataSource(endpoint1), NULL);
    GG_DataSource_SetDataSink(GG_CoapEndpoint_AsDataSource(endpoint2), NULL);
    GG_CoapEndpoint_Destroy(endpoint1);
    GG_CoapEndpoint_Destroy(endpoint2);
}

typedef struct {
    GG_IMPLEMENTS(GG_CoapRequestHandler);

    GG_TimerScheduler* scheduler;
    bool               was_called;
    uint8_t            last_message_code_handled;
    unsigned int       delay;
} AsyncHandler;

enum AsyncResponderMethod {
    ASYNC_RESPONDER_OWN_CONTEXT,
    ASYNC_RESPONDER_EXTERNAL_CONTEXT_TWO_STEPS,
    ASYNC_RESPONDER_EXTERNAL_CONTEXT_ONE_STEP
};

typedef struct {
    GG_IMPLEMENTS(GG_TimerListener);

    GG_Timer*             timer;
    AsyncHandler*         handler;
    GG_CoapEndpoint*      endpoint;
    const GG_CoapMessage* request;
    GG_CoapResponder*     responder;
    enum AsyncResponderMethod response_method;
} AsyncResponder;

static void
AsyncResponder_Destroy(AsyncResponder* self)
{
    GG_Timer_Destroy(self->timer);
    GG_CoapResponder_Release(self->responder);
    GG_FreeMemory(self);
}

static void
AsyncResponder_OnTimerFired(GG_TimerListener* _self, GG_Timer* timer, uint32_t time_elapsed)
{
    AsyncResponder* self = GG_SELF(AsyncResponder, GG_TimerListener);
    GG_COMPILER_UNUSED(timer);
    GG_COMPILER_UNUSED(time_elapsed);


    switch (self->response_method) {
        case ASYNC_RESPONDER_OWN_CONTEXT: {
            // create our response
            GG_CoapMessage* response = NULL;
            GG_Result result = GG_CoapEndpoint_CreateResponse(self->endpoint,
                                                              self->request,
                                                              GG_COAP_MESSAGE_CODE_CONTENT,
                                                              NULL, 0, NULL, 0,
                                                              &response);
            LONGS_EQUAL(GG_SUCCESS, result)

            // send the response using the responder
            result = GG_CoapResponder_SendResponse(self->responder, response);
            LONGS_EQUAL(GG_SUCCESS, result)

            // destroy the response
            GG_CoapMessage_Destroy(response);
            break;
        }

        case ASYNC_RESPONDER_EXTERNAL_CONTEXT_TWO_STEPS: {
            // create our response
            GG_CoapMessage* response = NULL;
            GG_Result result = GG_CoapResponder_CreateResponse(self->responder,
                                                               GG_COAP_MESSAGE_CODE_CONTENT,
                                                               NULL, 0, NULL, 0,
                                                               &response);
            LONGS_EQUAL(GG_SUCCESS, result)

            // send the response using the responder
            result = GG_CoapResponder_SendResponse(self->responder, response);
            LONGS_EQUAL(GG_SUCCESS, result)

            // destroy the response
            GG_CoapMessage_Destroy(response);
            break;
        }

        case ASYNC_RESPONDER_EXTERNAL_CONTEXT_ONE_STEP: {
            // respond using the responder
            GG_Result result = GG_CoapResponder_Respond(self->responder,
                                                        GG_COAP_MESSAGE_CODE_CONTENT,
                                                        NULL, 0, NULL, 0);
            LONGS_EQUAL(GG_SUCCESS, result)
            break;
        }
    }

    // we don't need to live anymore
    AsyncResponder_Destroy(self);
}

GG_IMPLEMENT_INTERFACE(AsyncResponder, GG_TimerListener) {
    AsyncResponder_OnTimerFired
};

static AsyncResponder*
AsyncResponder_Create(AsyncHandler*         handler,
                      GG_TimerScheduler*    scheduler,
                      GG_CoapEndpoint*      endpoint,
                      const GG_CoapMessage* request,
                      AsyncResponderMethod  response_method,
                      GG_CoapResponder*     responder)
{
    AsyncResponder* self = (AsyncResponder*)GG_AllocateZeroMemory(sizeof(AsyncResponder));

    GG_TimerScheduler_CreateTimer(scheduler, &self->timer);

    // init members
    self->endpoint        = endpoint;
    self->handler         = handler;
    self->request         = request;
    self->responder       = responder;
    self->response_method = response_method;

    // setup the interface
    GG_SET_INTERFACE(self, AsyncResponder, GG_TimerListener);

    return self;
}

static GG_Result
AsyncHandler_OnRequest(GG_CoapRequestHandler*   _self,
                       GG_CoapEndpoint*         endpoint,
                       const GG_CoapMessage*    request,
                       GG_CoapResponder*        responder,
                       const GG_BufferMetadata* transport_metadata,
                       GG_CoapMessage**         response)
{
    AsyncHandler* self = GG_SELF(AsyncHandler, GG_CoapRequestHandler);
    GG_COMPILER_UNUSED(transport_metadata);
    CHECK_TRUE(responder != NULL)

    self->was_called = true;
    self->last_message_code_handled = GG_CoapMessage_GetCode(request);

    if (self->delay) {
        // look at the request path to infer the response method we need to use
        GG_CoapMessageOption path_option;
        GG_Result result = GG_CoapMessage_GetOption(request, GG_COAP_MESSAGE_OPTION_URI_PATH, &path_option, 1);
        LONGS_EQUAL(GG_SUCCESS, result)
        AsyncResponderMethod response_method;
        if (!strncmp(path_option.value.string.chars, "1", 1)) {
            response_method = ASYNC_RESPONDER_OWN_CONTEXT;
        } else if (!strncmp(path_option.value.string.chars, "2", 1)) {
            response_method = ASYNC_RESPONDER_EXTERNAL_CONTEXT_ONE_STEP;
        } else if (!strncmp(path_option.value.string.chars, "3", 1)) {
            response_method = ASYNC_RESPONDER_EXTERNAL_CONTEXT_TWO_STEPS;
        } else {
            CHECK_TRUE(false)
            return GG_FAILURE;
        }

        // respond asynchronously
        AsyncResponder* async_responder = AsyncResponder_Create(self,
                                                                self->scheduler,
                                                                endpoint,
                                                                request,
                                                                response_method,
                                                                responder);
        GG_Timer_Schedule(async_responder->timer,
                          GG_CAST(async_responder, GG_TimerListener),
                          self->delay);
        return GG_ERROR_WOULD_BLOCK;
    } else {
        // respond synchronously
        return GG_CoapEndpoint_CreateResponse(endpoint,
                                              request,
                                              GG_COAP_MESSAGE_CODE_CONTENT,
                                              NULL, 0, NULL, 0,
                                              response);
    }
}

GG_IMPLEMENT_INTERFACE(AsyncHandler, GG_CoapRequestHandler) {
    .OnRequest = AsyncHandler_OnRequest
};

TEST(GG_COAP, Test_AsyncHandlers) {
    // create two endpoints and connect them together
    GG_TimerScheduler* timer_scheduler1 = NULL;
    GG_TimerScheduler_Create(&timer_scheduler1);
    GG_CoapEndpoint* endpoint1;
    GG_CoapEndpoint_Create(timer_scheduler1, NULL, NULL, &endpoint1);
    GG_CoapEndpoint* endpoint2;
    GG_CoapEndpoint_Create(timer_scheduler1, NULL, NULL, &endpoint2);
    GG_DataSource_SetDataSink(GG_CoapEndpoint_AsDataSource(endpoint1),
                              GG_CoapEndpoint_AsDataSink(endpoint2));
    GG_DataSource_SetDataSink(GG_CoapEndpoint_AsDataSource(endpoint2),
                              GG_CoapEndpoint_AsDataSink(endpoint1));

    // register an async handler
    AsyncHandler handler1;
    GG_SET_INTERFACE(&handler1, AsyncHandler, GG_CoapRequestHandler);
    handler1.last_message_code_handled = 0;
    handler1.was_called = false;
    handler1.delay = 1000;
    handler1.scheduler = timer_scheduler1;
    GG_Result result = GG_CoapEndpoint_RegisterRequestHandler(endpoint2,
                                                              "async1",
                                                              GG_COAP_REQUEST_HANDLER_FLAG_ALLOW_GET |
                                                              GG_COAP_REQUEST_HANDLER_FLAG_ENABLE_ASYNC,
                                                              GG_CAST(&handler1, GG_CoapRequestHandler));
    LONGS_EQUAL(GG_SUCCESS, result)

    // init a test client
    TestClient client1;

    const char* response_method_path[3] = {
        "1", "2", "3"
    };

    uint32_t now = 0;

    // try with async
    for (int i = 0; i < 3; i++) {
        // send a GET request
        TestClient_Init(&client1);
        GG_CoapMessageOptionParam options1[] = {
            GG_COAP_MESSAGE_OPTION_PARAM_STRING(URI_PATH, "async1"),
            GG_COAP_MESSAGE_OPTION_PARAM_STRING(URI_PATH, response_method_path[i]),
        };
        result = GG_CoapEndpoint_SendRequest(endpoint1,
                                             GG_COAP_METHOD_GET,
                                             options1, GG_ARRAY_SIZE(options1),
                                             NULL, 0,
                                             NULL,
                                             GG_CAST(&client1, GG_CoapResponseListener),
                                             &client1.request_handle);
        LONGS_EQUAL(GG_SUCCESS, result)

        // check that the handler was called
        CHECK_TRUE(handler1.was_called)

        // check that we did not receive a response yet
        CHECK_FALSE(client1.ack_received)
        CHECK_TRUE(client1.response == NULL)

        // advance the time so that the async timer fires
        now += handler1.delay + 100;
        GG_TimerScheduler_SetTime(timer_scheduler1, now);

        // check that we got valid response
        CHECK_TRUE(client1.ack_received)
        CHECK_TRUE(client1.response != NULL)
        LONGS_EQUAL(GG_COAP_MESSAGE_CODE_CONTENT, GG_CoapMessage_GetCode(client1.response))

        // clear some state
        TestClient_Cleanup(&client1);
        handler1.last_message_code_handled = 0;
        handler1.was_called = false;
    }

    // now try the handler with a sync response
    handler1.delay = 0;
    handler1.was_called = false;
    handler1.last_message_code_handled = 0;
    GG_CoapMessageOptionParam options1[] = {
        GG_COAP_MESSAGE_OPTION_PARAM_STRING(URI_PATH, "async1"),
    };

    // send a GET request
    TestClient_Init(&client1);
    result = GG_CoapEndpoint_SendRequest(endpoint1,
                                         GG_COAP_METHOD_GET,
                                         options1, GG_ARRAY_SIZE(options1),
                                         NULL, 0,
                                         NULL,
                                         GG_CAST(&client1, GG_CoapResponseListener),
                                         &client1.request_handle);
    LONGS_EQUAL(GG_SUCCESS, result)

    // check that the handler was called
    CHECK_TRUE(handler1.was_called)

    // check that we got valid response
    CHECK_TRUE(client1.ack_received)
    CHECK_TRUE(client1.response != NULL)
    LONGS_EQUAL(GG_COAP_MESSAGE_CODE_CONTENT, GG_CoapMessage_GetCode(client1.response))

    // cleanup
    GG_TimerScheduler_Destroy(timer_scheduler1);
    GG_DataSource_SetDataSink(GG_CoapEndpoint_AsDataSource(endpoint1), NULL);
    GG_DataSource_SetDataSink(GG_CoapEndpoint_AsDataSource(endpoint2), NULL);
    GG_CoapEndpoint_Destroy(endpoint1);
    GG_CoapEndpoint_Destroy(endpoint2);
}

TEST(GG_COAP, Test_PathSplitter) {
    GG_Result result;

    GG_CoapMessageOptionParam params[10];

    size_t params_count = 10;
    result = GG_Coap_SplitPathOrQuery("", '/', params, &params_count, GG_COAP_MESSAGE_OPTION_URI_PATH);
    LONGS_EQUAL(GG_SUCCESS, result);
    LONGS_EQUAL(params_count, 0);

    params_count = 10;
    result = GG_Coap_SplitPathOrQuery("/", '/', params, &params_count, GG_COAP_MESSAGE_OPTION_URI_PATH);
    LONGS_EQUAL(GG_SUCCESS, result);
    LONGS_EQUAL(params_count, 0);

    params_count = 10;
    result = GG_Coap_SplitPathOrQuery("foo", '/', params, &params_count, GG_COAP_MESSAGE_OPTION_URI_PATH);
    LONGS_EQUAL(GG_SUCCESS, result);
    LONGS_EQUAL(params_count, 1);
    MEMCMP_EQUAL("foo", params[0].option.value.string.chars, params[0].option.value.string.length);
    CHECK_TRUE(params[0].next == NULL);

    params_count = 10;
    result = GG_Coap_SplitPathOrQuery("/foo", '/', params, &params_count, GG_COAP_MESSAGE_OPTION_URI_PATH);
    LONGS_EQUAL(GG_SUCCESS, result);
    LONGS_EQUAL(params_count, 1);
    MEMCMP_EQUAL("foo", params[0].option.value.string.chars, params[0].option.value.string.length);
    CHECK_TRUE(params[0].next == NULL);

    params_count = 10;
    result = GG_Coap_SplitPathOrQuery("foo/", '/', params, &params_count, GG_COAP_MESSAGE_OPTION_URI_PATH);
    LONGS_EQUAL(GG_SUCCESS, result);
    LONGS_EQUAL(params_count, 1);
    MEMCMP_EQUAL("foo", params[0].option.value.string.chars, params[0].option.value.string.length);
    CHECK_TRUE(params[0].next == NULL);

    params_count = 10;
    result = GG_Coap_SplitPathOrQuery("foo//", '/', params, &params_count, GG_COAP_MESSAGE_OPTION_URI_PATH);
    LONGS_EQUAL(GG_ERROR_INVALID_SYNTAX, result);

    params_count = 10;
    result = GG_Coap_SplitPathOrQuery("foo//bar", '/', params, &params_count, GG_COAP_MESSAGE_OPTION_URI_PATH);
    LONGS_EQUAL(GG_ERROR_INVALID_SYNTAX, result);

    params_count = 0;
    result = GG_Coap_SplitPathOrQuery("foo", '/', params, &params_count, GG_COAP_MESSAGE_OPTION_URI_PATH);
    LONGS_EQUAL(GG_ERROR_NOT_ENOUGH_SPACE, result);

    params_count = 1;
    result = GG_Coap_SplitPathOrQuery("foo/bar", '/', params, &params_count, GG_COAP_MESSAGE_OPTION_URI_PATH);
    LONGS_EQUAL(GG_ERROR_NOT_ENOUGH_SPACE, result);

    params_count = 10;
    result = GG_Coap_SplitPathOrQuery("foo/bar/bla", '/', params, &params_count, GG_COAP_MESSAGE_OPTION_URI_PATH);
    LONGS_EQUAL(GG_SUCCESS, result);
    LONGS_EQUAL(params_count, 3);
    MEMCMP_EQUAL("foo", params[0].option.value.string.chars, params[0].option.value.string.length);
    CHECK_TRUE(params[0].next == NULL);
    MEMCMP_EQUAL("bar", params[1].option.value.string.chars, params[1].option.value.string.length);
    CHECK_TRUE(params[1].next == NULL);
    MEMCMP_EQUAL("bla", params[2].option.value.string.chars, params[2].option.value.string.length);
    CHECK_TRUE(params[2].next == NULL);

    params_count = 0;
    result = GG_Coap_SplitPathOrQuery("foo/bar/bla", '/', NULL, &params_count, GG_COAP_MESSAGE_OPTION_URI_PATH);
    LONGS_EQUAL(GG_SUCCESS, result);
    LONGS_EQUAL(3, params_count);
}

TEST(GG_COAP, Test_RequestHandlerRegistration) {
    GG_Result result;

    GG_CoapEndpoint* endpoint = NULL;

    AsyncHandler handler1 = {
        GG_INTERFACE_INITIALIZER(AsyncHandler, GG_CoapRequestHandler)
    };

    AsyncHandler handler2 = {
        GG_INTERFACE_INITIALIZER(AsyncHandler, GG_CoapRequestHandler)
    };

    AsyncHandler handler3 = {
        GG_INTERFACE_INITIALIZER(AsyncHandler, GG_CoapRequestHandler)
    };

    result = GG_CoapEndpoint_Create(timer_scheduler, NULL, NULL, &endpoint);
    LONGS_EQUAL(GG_SUCCESS, result);

    result = GG_CoapEndpoint_RegisterRequestHandler(endpoint, "foo", 0, GG_CAST(&handler1, GG_CoapRequestHandler));
    LONGS_EQUAL(GG_SUCCESS, result);

    result = GG_CoapEndpoint_RegisterRequestHandler(endpoint, "/bar", 0, GG_CAST(&handler2, GG_CoapRequestHandler));
    LONGS_EQUAL(GG_SUCCESS, result);

    result = GG_CoapEndpoint_RegisterRequestHandler(endpoint, "/bar2", 0, GG_CAST(&handler2, GG_CoapRequestHandler));
    LONGS_EQUAL(GG_SUCCESS, result);

    result = GG_CoapEndpoint_UnregisterRequestHandler(endpoint, "xxx", NULL);
    LONGS_EQUAL(GG_ERROR_NO_SUCH_ITEM, result);

    result = GG_CoapEndpoint_UnregisterRequestHandler(endpoint, NULL, GG_CAST(&handler3, GG_CoapRequestHandler));
    LONGS_EQUAL(GG_ERROR_NO_SUCH_ITEM, result);

    result = GG_CoapEndpoint_UnregisterRequestHandler(endpoint, "xxx", GG_CAST(&handler3, GG_CoapRequestHandler));
    LONGS_EQUAL(GG_ERROR_NO_SUCH_ITEM, result);

    result = GG_CoapEndpoint_UnregisterRequestHandler(endpoint, "xxx", GG_CAST(&handler1, GG_CoapRequestHandler));
    LONGS_EQUAL(GG_ERROR_NO_SUCH_ITEM, result);

    result = GG_CoapEndpoint_UnregisterRequestHandler(endpoint, "xxx", GG_CAST(&handler2, GG_CoapRequestHandler));
    LONGS_EQUAL(GG_ERROR_NO_SUCH_ITEM, result);

    result = GG_CoapEndpoint_UnregisterRequestHandler(endpoint, "foo", GG_CAST(&handler3, GG_CoapRequestHandler));
    LONGS_EQUAL(GG_ERROR_NO_SUCH_ITEM, result);

    result = GG_CoapEndpoint_UnregisterRequestHandler(endpoint, "/foo", NULL);
    LONGS_EQUAL(GG_SUCCESS, result);

    result = GG_CoapEndpoint_UnregisterRequestHandler(endpoint, "/foo", NULL);
    LONGS_EQUAL(GG_ERROR_NO_SUCH_ITEM, result);

    result = GG_CoapEndpoint_UnregisterRequestHandler(endpoint, "bar", GG_CAST(&handler2, GG_CoapRequestHandler));
    LONGS_EQUAL(GG_SUCCESS, result);

    result = GG_CoapEndpoint_UnregisterRequestHandler(endpoint, NULL, GG_CAST(&handler2, GG_CoapRequestHandler));
    LONGS_EQUAL(GG_SUCCESS, result);

    result = GG_CoapEndpoint_UnregisterRequestHandler(endpoint, NULL, GG_CAST(&handler2, GG_CoapRequestHandler));
    LONGS_EQUAL(GG_ERROR_NO_SUCH_ITEM, result);

    result = GG_CoapEndpoint_UnregisterRequestHandler(endpoint, NULL, NULL);
    LONGS_EQUAL(GG_ERROR_NO_SUCH_ITEM, result);

    result = GG_CoapEndpoint_RegisterRequestHandler(endpoint, "foo", 0, GG_CAST(&handler1, GG_CoapRequestHandler));
    LONGS_EQUAL(GG_SUCCESS, result);

    result = GG_CoapEndpoint_UnregisterRequestHandler(endpoint, NULL, NULL);
    LONGS_EQUAL(GG_SUCCESS, result);

    result = GG_CoapEndpoint_UnregisterRequestHandler(endpoint, NULL, NULL);
    LONGS_EQUAL(GG_ERROR_NO_SUCH_ITEM, result);

    GG_CoapEndpoint_Destroy(endpoint);
}

typedef struct {
    GG_IMPLEMENTS(GG_CoapRequestFilter);

    bool      was_invoked;
    GG_Result response_result;
    bool      create_response;
    uint8_t   response_payload[4];
} TestRequestFilter;

static GG_Result
TestRequestFilter_FilterRequest(GG_CoapRequestFilter* _self,
                                GG_CoapEndpoint*      endpoint,
                                uint32_t              handler_flags,
                                const GG_CoapMessage* request,
                                GG_CoapMessage**      response)
{
    TestRequestFilter* self = GG_SELF(TestRequestFilter, GG_CoapRequestFilter);
    GG_COMPILER_UNUSED(handler_flags);

    self->was_invoked = true;

    if (self->create_response) {
        return GG_CoapEndpoint_CreateResponse(endpoint,
                                              request,
                                              GG_COAP_MESSAGE_CODE_CONTENT,
                                              NULL, 0,
                                              self->response_payload, sizeof(self->response_payload),
                                              response);
    }

    return self->response_result;
}

GG_IMPLEMENT_INTERFACE(TestRequestFilter, GG_CoapRequestFilter) {
    .FilterRequest = TestRequestFilter_FilterRequest
};


TEST(GG_COAP, Test_RequestFilterRegistration) {
    GG_Result result;

    GG_CoapEndpoint* endpoint = NULL;

    TestRequestFilter filter;

    result = GG_CoapEndpoint_Create(timer_scheduler, NULL, NULL, &endpoint);
    LONGS_EQUAL(GG_SUCCESS, result);

    result = GG_CoapEndpoint_RegisterRequestFilter(endpoint, GG_CAST(&filter, GG_CoapRequestFilter));
    LONGS_EQUAL(GG_SUCCESS, result);

    result = GG_CoapEndpoint_UnregisterRequestFilter(endpoint, GG_CAST(&filter, GG_CoapRequestFilter));
    LONGS_EQUAL(GG_SUCCESS, result);

    result = GG_CoapEndpoint_UnregisterRequestFilter(endpoint, GG_CAST(&filter, GG_CoapRequestFilter));
    LONGS_EQUAL(GG_ERROR_NO_SUCH_ITEM, result);

    GG_CoapEndpoint_Destroy(endpoint);
}

TEST(GG_COAP, Test_RequestFilters) {
    // create two endpoints and connect them together
    GG_TimerScheduler* timer_scheduler1 = NULL;
    GG_TimerScheduler_Create(&timer_scheduler1);
    GG_CoapEndpoint* endpoint1;
    GG_CoapEndpoint_Create(timer_scheduler1, NULL, NULL, &endpoint1);
    GG_CoapEndpoint* endpoint2;
    GG_CoapEndpoint_Create(timer_scheduler1, NULL, NULL, &endpoint2);
    GG_DataSource_SetDataSink(GG_CoapEndpoint_AsDataSource(endpoint1),
                              GG_CoapEndpoint_AsDataSink(endpoint2));
    GG_DataSource_SetDataSink(GG_CoapEndpoint_AsDataSource(endpoint2),
                              GG_CoapEndpoint_AsDataSink(endpoint1));

    // register a test handler
    TestHandler handler1;
    GG_SET_INTERFACE(&handler1, TestHandler, GG_CoapRequestHandler);
    handler1.result_to_return = GG_COAP_MESSAGE_CODE_CREATED;
    handler1.code_to_respond_with = 0;
    handler1.last_message_code_handled = 0;
    handler1.was_called = false;
    GG_Result result = GG_CoapEndpoint_RegisterRequestHandler(endpoint2,
                                                              "foo",
                                                              GG_COAP_REQUEST_HANDLER_FLAG_ALLOW_GET,
                                                              GG_CAST(&handler1, GG_CoapRequestHandler));
    LONGS_EQUAL(GG_SUCCESS, result);

    // init a test client
    TestClient client1;
    TestClient_Init(&client1);

    // send a GET request (should not be filtered out)
    GG_CoapMessageOptionParam options[] = {
        GG_COAP_MESSAGE_OPTION_PARAM_STRING(URI_PATH, "foo"),
    };
    TestClient_Cleanup(&client1);
    result = GG_CoapEndpoint_SendRequest(endpoint1,
                                         GG_COAP_METHOD_GET,
                                         options, GG_ARRAY_SIZE(options),
                                         NULL, 0,
                                         NULL,
                                         GG_CAST(&client1, GG_CoapResponseListener),
                                         &client1.request_handle);
    LONGS_EQUAL(GG_SUCCESS, result);

    // check that the handler was called
    CHECK_TRUE(handler1.was_called);

    // check that we got valid response
    CHECK_TRUE(client1.ack_received);
    CHECK_TRUE(client1.response != NULL);
    LONGS_EQUAL(GG_COAP_MESSAGE_CODE_CREATED, GG_CoapMessage_GetCode(client1.response));

    // init a filter
    TestRequestFilter filter1 = {
        GG_INTERFACE_INITIALIZER(TestRequestFilter, GG_CoapRequestFilter)
    };

    // init a second filter
    TestRequestFilter filter2 = {
        GG_INTERFACE_INITIALIZER(TestRequestFilter, GG_CoapRequestFilter)
    };

    // register the filters with the endpoint
    result = GG_CoapEndpoint_RegisterRequestFilter(endpoint2, GG_CAST(&filter1, GG_CoapRequestFilter));
    LONGS_EQUAL(GG_SUCCESS, result);
    result = GG_CoapEndpoint_RegisterRequestFilter(endpoint2, GG_CAST(&filter2, GG_CoapRequestFilter));
    LONGS_EQUAL(GG_SUCCESS, result);

    // send a request
    result = GG_CoapEndpoint_SendRequest(endpoint1,
                                         GG_COAP_METHOD_GET,
                                         options, GG_ARRAY_SIZE(options),
                                         NULL, 0,
                                         NULL,
                                         GG_CAST(&client1, GG_CoapResponseListener),
                                         &client1.request_handle);
    LONGS_EQUAL(GG_SUCCESS, result);

    // check that both filters have been invoked
    CHECK_TRUE(filter1.was_invoked);
    CHECK_TRUE(filter2.was_invoked);

    // reset
    filter1.was_invoked = false;
    filter2.was_invoked = false;

    // make the first filter respond with an error
    filter1.response_result = GG_ERROR_NO_SUCH_ITEM;

    // send a request
    result = GG_CoapEndpoint_SendRequest(endpoint1,
                                         GG_COAP_METHOD_GET,
                                         options, GG_ARRAY_SIZE(options),
                                         NULL, 0,
                                         NULL,
                                         GG_CAST(&client1, GG_CoapResponseListener),
                                         &client1.request_handle);
    LONGS_EQUAL(GG_SUCCESS, result);

    // check that only filter1 was invoked
    CHECK_TRUE(filter1.was_invoked);
    CHECK_FALSE(filter2.was_invoked);

    // check the response
    CHECK_TRUE(client1.ack_received);
    CHECK_TRUE(client1.response != NULL);
    LONGS_EQUAL(GG_COAP_MESSAGE_CODE_INTERNAL_SERVER_ERROR, GG_CoapMessage_GetCode(client1.response));
    client1.ack_received = false;

    // make the first filter respond with a CoAP result
    filter1.was_invoked = false;
    filter2.was_invoked = false;
    filter1.response_result = GG_COAP_MESSAGE_CODE_UNAUTHORIZED;

    // send a request
    result = GG_CoapEndpoint_SendRequest(endpoint1,
                                         GG_COAP_METHOD_GET,
                                         options, GG_ARRAY_SIZE(options),
                                         NULL, 0,
                                         NULL,
                                         GG_CAST(&client1, GG_CoapResponseListener),
                                         &client1.request_handle);
    LONGS_EQUAL(GG_SUCCESS, result);

    // check that only filter1 was invoked
    CHECK_TRUE(filter1.was_invoked);
    CHECK_FALSE(filter2.was_invoked);

    // check the response
    CHECK_TRUE(client1.ack_received);
    CHECK_TRUE(client1.response != NULL);
    LONGS_EQUAL(GG_COAP_MESSAGE_CODE_UNAUTHORIZED, GG_CoapMessage_GetCode(client1.response));
    client1.ack_received = false;

    // make the first filter respond with a CoAP response
    filter1.was_invoked = false;
    filter2.was_invoked = false;
    filter1.response_result = 0;
    filter1.create_response = true;
    filter1.response_payload[0] = 1;
    filter1.response_payload[1] = 2;
    filter1.response_payload[2] = 3;
    filter1.response_payload[3] = 4;

    // send a request
    result = GG_CoapEndpoint_SendRequest(endpoint1,
                                         GG_COAP_METHOD_GET,
                                         options, GG_ARRAY_SIZE(options),
                                         NULL, 0,
                                         NULL,
                                         GG_CAST(&client1, GG_CoapResponseListener),
                                         &client1.request_handle);
    LONGS_EQUAL(GG_SUCCESS, result);

    // check that only filter1 was invoked
    CHECK_TRUE(filter1.was_invoked);
    CHECK_FALSE(filter2.was_invoked);

    // check the response
    CHECK_TRUE(client1.ack_received);
    CHECK_TRUE(client1.response != NULL);
    LONGS_EQUAL(GG_COAP_MESSAGE_CODE_CONTENT, GG_CoapMessage_GetCode(client1.response));
    client1.ack_received = false;
    LONGS_EQUAL(sizeof(filter1.response_payload), GG_CoapMessage_GetPayloadSize(client1.response));
    MEMCMP_EQUAL(filter1.response_payload,
                 GG_CoapMessage_GetPayload(client1.response),
                 sizeof(filter1.response_payload));

    // cleanup
    TestClient_Cleanup(&client1);
    GG_DataSource_SetDataSink(GG_CoapEndpoint_AsDataSource(endpoint1), NULL);
    GG_DataSource_SetDataSink(GG_CoapEndpoint_AsDataSource(endpoint2), NULL);
    GG_CoapEndpoint_Destroy(endpoint1);
    GG_CoapEndpoint_Destroy(endpoint2);
}

TEST(GG_COAP, Test_GroupRequestFilters) {
    // create two endpoints and connect them together
    GG_TimerScheduler* timer_scheduler1 = NULL;
    GG_TimerScheduler_Create(&timer_scheduler1);
    GG_CoapEndpoint* endpoint1;
    GG_CoapEndpoint_Create(timer_scheduler1, NULL, NULL, &endpoint1);
    GG_CoapEndpoint* endpoint2;
    GG_CoapEndpoint_Create(timer_scheduler1, NULL, NULL, &endpoint2);
    GG_DataSource_SetDataSink(GG_CoapEndpoint_AsDataSource(endpoint1),
                              GG_CoapEndpoint_AsDataSink(endpoint2));
    GG_DataSource_SetDataSink(GG_CoapEndpoint_AsDataSource(endpoint2),
                              GG_CoapEndpoint_AsDataSink(endpoint1));

    // register a test handler in group 1 and 3
    TestHandler handler1;
    GG_SET_INTERFACE(&handler1, TestHandler, GG_CoapRequestHandler);
    handler1.result_to_return = GG_COAP_MESSAGE_CODE_CONTENT;
    handler1.code_to_respond_with = 0;
    handler1.last_message_code_handled = 0;
    handler1.was_called = false;
    GG_Result result = GG_CoapEndpoint_RegisterRequestHandler(endpoint2,
                                                              "foo",
                                                              GG_COAP_REQUEST_HANDLER_FLAG_ALLOW_GET |
                                                              GG_COAP_REQUEST_HANDLER_FLAG_GROUP_1   |
                                                              GG_COAP_REQUEST_HANDLER_FLAG_GROUP_3,
                                                              GG_CAST(&handler1, GG_CoapRequestHandler));
    LONGS_EQUAL(GG_SUCCESS, result);

    // register a test handler in no group (other than 0)
    TestHandler handler2;
    GG_SET_INTERFACE(&handler2, TestHandler, GG_CoapRequestHandler);
    handler2.result_to_return = GG_COAP_MESSAGE_CODE_CONTENT;
    handler2.code_to_respond_with = 0;
    handler2.last_message_code_handled = 0;
    handler2.was_called = false;
    result = GG_CoapEndpoint_RegisterRequestHandler(endpoint2,
                                                    "bar",
                                                    GG_COAP_REQUEST_HANDLER_FLAG_ALLOW_GET,
                                                    GG_CAST(&handler2, GG_CoapRequestHandler));
    LONGS_EQUAL(GG_SUCCESS, result);

    // init a test client
    TestClient client1;
    TestClient_Init(&client1);

    // create a group filter
    GG_CoapGroupRequestFilter* group_filter = NULL;
    result = GG_CoapGroupRequestFilter_Create(&group_filter);
    LONGS_EQUAL(GG_SUCCESS, result);
    LONGS_EQUAL(0, GG_CoapGroupRequestFilter_GetGroup(group_filter));

    // register the filter with the endpoint
    result = GG_CoapEndpoint_RegisterRequestFilter(endpoint2,
                                                   GG_CoapGroupRequestFilter_AsCoapRequestFilter(group_filter));
    LONGS_EQUAL(GG_SUCCESS, result);

    // send a GET request (should not be filtered out)
    GG_CoapMessageOptionParam options1[] = {
        GG_COAP_MESSAGE_OPTION_PARAM_STRING(URI_PATH, "foo"),
    };
    GG_CoapMessageOptionParam options2[] = {
        GG_COAP_MESSAGE_OPTION_PARAM_STRING(URI_PATH, "bar"),
    };

    // send a request for /foo
    result = GG_CoapEndpoint_SendRequest(endpoint1,
                                         GG_COAP_METHOD_GET,
                                         options1, GG_ARRAY_SIZE(options1),
                                         NULL, 0,
                                         NULL,
                                         GG_CAST(&client1, GG_CoapResponseListener),
                                         &client1.request_handle);
    LONGS_EQUAL(GG_SUCCESS, result);

    // check that the handler was called
    CHECK_TRUE(handler1.was_called);

    // send a request for /bar
    TestClient_Cleanup(&client1);
    result = GG_CoapEndpoint_SendRequest(endpoint1,
                                         GG_COAP_METHOD_GET,
                                         options2, GG_ARRAY_SIZE(options2),
                                         NULL, 0,
                                         NULL,
                                         GG_CAST(&client1, GG_CoapResponseListener),
                                         &client1.request_handle);
    LONGS_EQUAL(GG_SUCCESS, result);

    // check that the handler was called
    CHECK_TRUE(handler2.was_called);

    // reset some flags
    handler1.was_called = false;
    handler2.was_called = false;

    // change the filter's group to group 2
    GG_CoapGroupRequestFilter_SetGroup(group_filter, 2);
    LONGS_EQUAL(2, GG_CoapGroupRequestFilter_GetGroup(group_filter));

    // send a request for /foo
    TestClient_Cleanup(&client1);
    result = GG_CoapEndpoint_SendRequest(endpoint1,
                                         GG_COAP_METHOD_GET,
                                         options1, GG_ARRAY_SIZE(options1),
                                         NULL, 0,
                                         NULL,
                                         GG_CAST(&client1, GG_CoapResponseListener),
                                         &client1.request_handle);
    LONGS_EQUAL(GG_SUCCESS, result);

    // check that the handler was not called
    CHECK_FALSE(handler1.was_called);
    CHECK_TRUE(client1.response != NULL);
    LONGS_EQUAL(GG_COAP_MESSAGE_CODE_UNAUTHORIZED, GG_CoapMessage_GetCode(client1.response));

    // send a request for /bar
    TestClient_Cleanup(&client1);
    result = GG_CoapEndpoint_SendRequest(endpoint1,
                                         GG_COAP_METHOD_GET,
                                         options2, GG_ARRAY_SIZE(options2),
                                         NULL, 0,
                                         NULL,
                                         GG_CAST(&client1, GG_CoapResponseListener),
                                         &client1.request_handle);
    LONGS_EQUAL(GG_SUCCESS, result);

    // check that the handler was not called
    CHECK_FALSE(handler2.was_called);
    CHECK_TRUE(client1.response != NULL);
    LONGS_EQUAL(GG_COAP_MESSAGE_CODE_UNAUTHORIZED, GG_CoapMessage_GetCode(client1.response));

    // reset some flags
    handler1.was_called = false;
    handler2.was_called = false;

    // change the filter's group to group 3
    GG_CoapGroupRequestFilter_SetGroup(group_filter, 3);
    LONGS_EQUAL(3, GG_CoapGroupRequestFilter_GetGroup(group_filter));

    // send a request for /foo
    TestClient_Cleanup(&client1);
    result = GG_CoapEndpoint_SendRequest(endpoint1,
                                         GG_COAP_METHOD_GET,
                                         options1, GG_ARRAY_SIZE(options1),
                                         NULL, 0,
                                         NULL,
                                         GG_CAST(&client1, GG_CoapResponseListener),
                                         &client1.request_handle);
    LONGS_EQUAL(GG_SUCCESS, result);

    // check that the handler was called
    CHECK_TRUE(handler1.was_called);

    // send a request for /bar
    TestClient_Cleanup(&client1);
    result = GG_CoapEndpoint_SendRequest(endpoint1,
                                         GG_COAP_METHOD_GET,
                                         options2, GG_ARRAY_SIZE(options2),
                                         NULL, 0,
                                         NULL,
                                         GG_CAST(&client1, GG_CoapResponseListener),
                                         &client1.request_handle);
    LONGS_EQUAL(GG_SUCCESS, result);

    // check that the handler was not called
    CHECK_FALSE(handler2.was_called);
    CHECK_TRUE(client1.response != NULL);
    LONGS_EQUAL(GG_COAP_MESSAGE_CODE_UNAUTHORIZED, GG_CoapMessage_GetCode(client1.response));

    // reset some flags
    handler1.was_called = false;
    handler2.was_called = false;

    // cleanup
    TestClient_Cleanup(&client1);
    GG_DataSource_SetDataSink(GG_CoapEndpoint_AsDataSource(endpoint1), NULL);
    GG_DataSource_SetDataSink(GG_CoapEndpoint_AsDataSource(endpoint2), NULL);
    GG_CoapEndpoint_Destroy(endpoint1);
    GG_CoapEndpoint_Destroy(endpoint2);
    GG_CoapGroupRequestFilter_Destroy(group_filter);
}

TEST(GG_COAP, Test_CloneOptions) {
    uint8_t etag[3] = {1, 2, 3};
    GG_CoapMessageOptionParam options[5] = {
        GG_COAP_MESSAGE_OPTION_PARAM_EMPTY(IF_NONE_MATCH),
        GG_COAP_MESSAGE_OPTION_PARAM_UINT(URI_PORT, 5683),
        GG_COAP_MESSAGE_OPTION_PARAM_STRING(URI_PATH, "hello"),
        GG_COAP_MESSAGE_OPTION_PARAM_STRING_L(URI_PATH, "bye bye", 7),
        GG_COAP_MESSAGE_OPTION_PARAM_OPAQUE(ETAG, etag, sizeof(etag))
    };

    GG_CoapMessageOptionParam options2[2] = {
        GG_COAP_MESSAGE_OPTION_PARAM_UINT(URI_PORT, 5684),
        GG_COAP_MESSAGE_OPTION_PARAM_STRING(URI_PATH, "foobar")
    };

    // chain options
    options[4].next = &options2[0];

    GG_CoapMessageOptionParam* clone = GG_Coap_CloneOptions(options, GG_ARRAY_SIZE(options) + GG_ARRAY_SIZE(options2));
    CHECK_TRUE(clone != NULL);

    for (unsigned int i = 0; i < GG_ARRAY_SIZE(options) + GG_ARRAY_SIZE(options2); i++) {
        GG_CoapMessageOptionParam* option =
            i < GG_ARRAY_SIZE(options) ? &options[i] : &options2[i - GG_ARRAY_SIZE(options)];
        LONGS_EQUAL(option->option.type, clone[i].option.type);
        switch (option->option.type) {
            case GG_COAP_MESSAGE_OPTION_TYPE_UINT:
                LONGS_EQUAL(option->option.value.uint, clone[i].option.value.uint);
                break;

            case GG_COAP_MESSAGE_OPTION_TYPE_STRING:
                if (option->option.value.string.length) {
                    LONGS_EQUAL(option->option.value.string.length, clone[i].option.value.string.length);
                } else {
                    LONGS_EQUAL(strlen(option->option.value.string.chars), clone[i].option.value.string.length);
                }
                MEMCMP_EQUAL(option->option.value.string.chars,
                             clone[i].option.value.string.chars,
                             clone[i].option.value.string.length);
                break;

            case GG_COAP_MESSAGE_OPTION_TYPE_OPAQUE:
                LONGS_EQUAL(option->option.value.opaque.size, clone[i].option.value.opaque.size);
                MEMCMP_EQUAL(option->option.value.opaque.bytes,
                             clone[i].option.value.opaque.bytes,
                             option->option.value.opaque.size);
                break;

            case GG_COAP_MESSAGE_OPTION_TYPE_EMPTY:
                break;
        }
    }

    GG_FreeMemory(clone);
}

TEST(GG_COAP, Test_ExtendedErrors) {
    GG_CoapExtendedError error_in;
    GG_CoapExtendedError error_out;
    GG_Result result;

    error_in.name_space = NULL;
    error_in.name_space_size = 0;
    error_in.message = NULL;
    error_in.message_size = 0;
    error_in.code = 1234;
    size_t encoded_size = GG_CoapExtendedError_GetEncodedSize(&error_in);
    LONGS_EQUAL(3, encoded_size);

    error_in.name_space = "hello";
    encoded_size = GG_CoapExtendedError_GetEncodedSize(&error_in);
    LONGS_EQUAL(10, encoded_size);

    error_in.name_space = "hello";
    error_in.name_space_size = 3;
    encoded_size = GG_CoapExtendedError_GetEncodedSize(&error_in);
    LONGS_EQUAL(8, encoded_size);

    error_in.message = "foo";
    encoded_size = GG_CoapExtendedError_GetEncodedSize(&error_in);
    LONGS_EQUAL(13, encoded_size);

    error_in.message_size = 3;
    encoded_size = GG_CoapExtendedError_GetEncodedSize(&error_in);
    LONGS_EQUAL(13, encoded_size);

    uint8_t msg1[] = {
        0x0a, 0x0f, 0x6f, 0x72, 0x67, 0x2e, 0x65, 0x78,
        0x61, 0x6d, 0x70, 0x6c, 0x65, 0x2e, 0x66, 0x6f,
        0x6f, 0x10, 0xab, 0x02, 0x1a, 0x05, 0x68, 0x65,
        0x6c, 0x6c, 0x6f
    };

    // empty messages are allowed
    error_out = error_in; // copy some non-zero values
    result = GG_CoapExtendedError_Decode(&error_out, msg1, 0);
    LONGS_EQUAL(GG_SUCCESS, result);
    LONGS_EQUAL(0, error_out.code);
    LONGS_EQUAL(0, error_out.name_space_size);
    LONGS_EQUAL(0, error_out.message_size);

    // invalid protobuf
    result = GG_CoapExtendedError_Decode(&error_out, msg1, 1);
    LONGS_EQUAL(GG_ERROR_INVALID_FORMAT, result);

    // normal decode
    result = GG_CoapExtendedError_Decode(&error_out, msg1, sizeof(msg1));
    LONGS_EQUAL(GG_SUCCESS, result);
    LONGS_EQUAL(-150, error_out.code);
    LONGS_EQUAL(15, error_out.name_space_size);
    STRNCMP_EQUAL("org.example.foo", error_out.name_space, error_out.name_space_size);
    LONGS_EQUAL(5, error_out.message_size);
    STRNCMP_EQUAL("hello", error_out.message, error_out.message_size);

    // normal encode
    uint8_t buffer[27];
    LONGS_EQUAL(sizeof(buffer), sizeof(msg1));
    encoded_size = GG_CoapExtendedError_GetEncodedSize(&error_out);
    LONGS_EQUAL(sizeof(buffer), encoded_size);
    result = GG_CoapExtendedError_Encode(&error_out, buffer);
    LONGS_EQUAL(GG_SUCCESS, result);
    MEMCMP_EQUAL(msg1, buffer, sizeof(msg1));

    // protobuf with extra/unknown fields
    uint8_t msg2[] = {
        0x0a, 0x0f, 0x6f, 0x72, 0x67, 0x2e, 0x65, 0x78,
        0x61, 0x6d, 0x70, 0x6c, 0x65, 0x2e, 0x66, 0x6f,
        0x6f, 0x10, 0xab, 0x02, 0x1a, 0x05, 0x68, 0x65,
        0x6c, 0x6c, 0x6f, 0x25, 0xd2, 0x04, 0x00, 0x00,
        0x29, 0x2e, 0x16, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x32, 0x03, 0x62, 0x61, 0x72
    };
    result = GG_CoapExtendedError_Decode(&error_out, msg2, sizeof(msg2));
    LONGS_EQUAL(GG_SUCCESS, result);
    LONGS_EQUAL(-150, error_out.code);
    LONGS_EQUAL(15, error_out.name_space_size);
    STRNCMP_EQUAL("org.example.foo", error_out.name_space, error_out.name_space_size);
    LONGS_EQUAL(5, error_out.message_size);
    STRNCMP_EQUAL("hello", error_out.message, error_out.message_size);
}

TEST(GG_COAP, Test_TokenPrefix)
{
    GG_TimerScheduler* scheduler;
    GG_Result result = GG_TimerScheduler_Create(&scheduler);
    LONGS_EQUAL(GG_SUCCESS, result);

    GG_MemoryDataSink* sink;
    result = GG_MemoryDataSink_Create(&sink);

    GG_CoapEndpoint* endpoint;
    result = GG_CoapEndpoint_Create(scheduler, GG_MemoryDataSink_AsDataSink(sink), NULL, &endpoint);
    LONGS_EQUAL(GG_SUCCESS, result);

    uint8_t prefix[3] = {0x03, 0x05, 0x07};
    result = GG_CoapEndpoint_SetTokenPrefix(endpoint, prefix, sizeof(prefix));
    LONGS_EQUAL(GG_SUCCESS, result);

    size_t prefix_size = 0;
    const uint8_t* prefix_bytes = GG_CoapEndpoint_GetTokenPrefix(endpoint, &prefix_size);
    CHECK_TRUE(prefix_bytes != NULL);
    LONGS_EQUAL(sizeof(prefix), prefix_size);
    MEMCMP_EQUAL(prefix, prefix_bytes, prefix_size);

    result = GG_CoapEndpoint_SendRequest(endpoint, GG_COAP_METHOD_GET, NULL, 0, NULL, 0, NULL, NULL, NULL);
    LONGS_EQUAL(GG_SUCCESS, result);

    GG_CoapMessage* message;
    GG_Buffer* message_buffer = GG_MemoryDataSink_GetBuffer(sink);
    result = GG_CoapMessage_CreateFromDatagram(message_buffer, &message);
    LONGS_EQUAL(GG_SUCCESS, result);
    uint8_t token[GG_COAP_MESSGAGE_MAX_TOKEN_LENGTH];
    size_t token_size = GG_CoapMessage_GetToken(message, token);
    CHECK_TRUE(token_size >= sizeof(prefix));
    MEMCMP_EQUAL(prefix, token, sizeof(prefix));

    GG_CoapMessage_Destroy(message);
    GG_CoapEndpoint_Destroy(endpoint);
    GG_MemoryDataSink_Destroy(sink);
    GG_TimerScheduler_Destroy(scheduler);
}

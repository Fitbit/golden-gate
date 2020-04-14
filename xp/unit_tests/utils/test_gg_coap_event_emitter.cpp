#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

#include "CppUTest/MemoryLeakDetectorNewMacros.h"

#include <string.h>

#include "xp/module/gg_module.h"
#include "xp/common/gg_common.h"
#include "xp/utils/gg_async_pipe.h"
#include "xp/utils/gg_coap_event_emitter.h"

TEST_GROUP(GG_COAP_EVENT_EMITTER)
{
    void setup(void) {
    }

    void teardown(void) {
    }
};

typedef struct {
    GG_IMPLEMENTS(GG_CoapRequestHandler);

    bool      drop;
    bool      decode_error;
    size_t    call_count;
    uint8_t   last_message_code_handled;
    GG_Result result_to_return;
    uint32_t  received_events[32];
    size_t    received_events_count;
    uint8_t   received_payload[64]; // max 64 bytes here
    size_t    received_payload_size;
} TestHandler;

static uint32_t decode_varint(const uint8_t* varint, size_t* consumed)
{
    uint32_t result = 0;
    *consumed = 0;
    unsigned int shifts = 0;
    do {
        result |= (*varint & 0x7F) << shifts;
        ++*consumed;
        shifts += 7;
    } while (*varint++ & 0x80);
    return result;
}

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
    GG_COMPILER_UNUSED(endpoint);
    GG_COMPILER_UNUSED(transport_metadata);
    GG_COMPILER_UNUSED(response);

    ++self->call_count;
    self->decode_error = false;

    // store the received payload
    const uint8_t* payload = GG_CoapMessage_GetPayload(request);
    size_t payload_size = GG_CoapMessage_GetPayloadSize(request);
    self->received_payload_size = payload_size;
    if (payload_size) {
        memcpy(self->received_payload, payload, GG_MIN(sizeof(self->received_payload), payload_size));
    }

    // decode the list of events
    self->received_events_count = 0;
    while (payload_size >= 2) {
        size_t consumed = 0;
        // look for a field with ID == 1 and type == 0 (varint)
        if (payload[0] != (1 <<3)) {
            break;
        }
        ++payload;
        --payload_size;
        uint32_t event = decode_varint(payload, &consumed);
        payload += consumed;
        if (consumed > payload_size) {
            self->decode_error = true;
            break;
        }
        payload_size -= consumed;

        if (self->received_events_count < GG_ARRAY_SIZE(self->received_events)) {
            self->received_events[self->received_events_count++] = event;
        }
    }

    if (self->drop) {
        GG_CoapResponder_Release(responder);
        return GG_ERROR_WOULD_BLOCK;
    }

    return self->result_to_return;
}

GG_IMPLEMENT_INTERFACE(TestHandler, GG_CoapRequestHandler) {
    .OnRequest = TestHandler_OnRequest
};

TEST(GG_COAP_EVENT_EMITTER, Test_EventEmitter) {
    GG_Result result;

    GG_TimerScheduler* scheduler = NULL;
    result = GG_TimerScheduler_Create(&scheduler);
    LONGS_EQUAL(GG_SUCCESS, result);

    // create a client endpoint
    GG_CoapEndpoint* client_endpoint = NULL;
    GG_CoapEndpoint_Create(scheduler, NULL, NULL, &client_endpoint);
    LONGS_EQUAL(GG_SUCCESS, result);

    // create a server endpoint
    GG_CoapEndpoint* server_endpoint = NULL;
    GG_CoapEndpoint_Create(scheduler, NULL, NULL, &server_endpoint);
    LONGS_EQUAL(GG_SUCCESS, result);

    // connect the client and server endpoints together through an async pipe
    GG_AsyncPipe* pipe;
    result = GG_AsyncPipe_Create(scheduler, 1, &pipe);
    GG_DataSource_SetDataSink(GG_CoapEndpoint_AsDataSource(server_endpoint),
                              GG_AsyncPipe_AsDataSink(pipe));
    GG_DataSource_SetDataSink(GG_AsyncPipe_AsDataSource(pipe),
                              GG_CoapEndpoint_AsDataSink(client_endpoint));
    GG_DataSource_SetDataSink(GG_CoapEndpoint_AsDataSource(client_endpoint),
                              GG_CoapEndpoint_AsDataSink(server_endpoint));

    // register a test handler
    TestHandler handler;
    GG_SET_INTERFACE(&handler, TestHandler, GG_CoapRequestHandler);
    handler.result_to_return = GG_COAP_MESSAGE_CODE_CHANGED;
    handler.last_message_code_handled = 0;
    handler.drop = false;
    handler.received_events_count = 0;
    handler.decode_error = false;
    handler.call_count = 0;
    result = GG_CoapEndpoint_RegisterRequestHandler(server_endpoint,
                                                    "foo/bar",
                                                    GG_COAP_REQUEST_HANDLER_FLAG_ALLOW_POST |
                                                    GG_COAP_REQUEST_HANDLER_FLAG_ENABLE_ASYNC,
                                                    GG_CAST(&handler, GG_CoapRequestHandler));

    GG_CoapEventEmitter* emitter = NULL;
    result = GG_CoapEventEmitter_Create(client_endpoint, "foo/bar", scheduler, 4, 0, 0, &emitter);
    LONGS_EQUAL(GG_SUCCESS, result);

    // set an event now
    result = GG_CoapEventEmitter_SetEvent(emitter, GG_4CC('e', 'v', 't', '0'), 0);
    LONGS_EQUAL(GG_SUCCESS, result);
    LONGS_EQUAL(1, handler.call_count);
    LONGS_EQUAL(1, handler.received_events_count);
    LONGS_EQUAL(GG_4CC('e', 'v', 't', '0'), handler.received_events[0]);
    handler.call_count = 0;
    handler.received_events_count = 0;

    // make the server drop packets for a while
    handler.drop = true;

    // set an event now
    result = GG_CoapEventEmitter_SetEvent(emitter, GG_4CC('e', 'v', 't', '0'), 0);

    // advance the time by small increments for a total of 1000 seconds
    for (unsigned int i = 0; i < 1000; i++) {
        GG_TimerScheduler_SetTime(scheduler, 1000 * i);
    }

    // should have received the event, even though we never responded
    CHECK_TRUE(handler.call_count != 0);
    LONGS_EQUAL(1, handler.received_events_count);
    LONGS_EQUAL(GG_4CC('e', 'v', 't', '0'), handler.received_events[0]);
    handler.call_count = 0;
    handler.received_events_count = 0;

    // now stop dropping and advance by another 1000 seconds
    handler.drop = false;
    for (unsigned int i = 1000; i < 2000; i++) {
        GG_TimerScheduler_SetTime(scheduler, 1000 * i);
    }

    // we should have received the event now
    LONGS_EQUAL(1, handler.call_count);
    LONGS_EQUAL(1, handler.received_events_count);
    LONGS_EQUAL(GG_4CC('e', 'v', 't', '0'), handler.received_events[0]);
    CHECK_FALSE(GG_CoapEventEmitter_EventIsSet(emitter, GG_4CC('e', 'v', 't', '0')));
    handler.call_count = 0;
    handler.received_events_count = 0;

    // reset the time to some non-zero origin
    const uint32_t origin = 10000;
    GG_TimerScheduler_SetTime(scheduler, origin);

    // set an event with a latency of 1s and a second one with a latency of 2s
    result = GG_CoapEventEmitter_SetEvent(emitter, GG_4CC('e', 'v', 't', '0'), 1000);
    LONGS_EQUAL(GG_SUCCESS, result);
    result = GG_CoapEventEmitter_SetEvent(emitter, GG_4CC('e', 'v', 't', '1'), 2000);
    LONGS_EQUAL(GG_SUCCESS, result);

    // no event should have been sent yet
    LONGS_EQUAL(0, handler.call_count);
    LONGS_EQUAL(0, handler.received_events_count);

    // advance the time a bit, but not enough to flush any event
    GG_TimerScheduler_SetTime(scheduler, origin + 500);

    // no event should have been sent yet
    LONGS_EQUAL(0, handler.call_count);
    LONGS_EQUAL(0, handler.received_events_count);

    // now advance to some time after the first event's max latency
    GG_TimerScheduler_SetTime(scheduler, origin + 1500);

    // both events should have been delivered (in any order in the list)
    LONGS_EQUAL(2, handler.received_events_count);
    CHECK_TRUE(handler.received_events[0] == GG_4CC('e', 'v', 't', '0') ||
               handler.received_events[1] == GG_4CC('e', 'v', 't', '0'));
    CHECK_TRUE(handler.received_events[0] == GG_4CC('e', 'v', 't', '1') ||
               handler.received_events[1] == GG_4CC('e', 'v', 't', '1'));
    handler.call_count = 0;
    handler.received_events_count = 0;

    // check the protobuf encoding of the event set
    const uint8_t protobuf[] =  {0x08, 0xb0, 0xe8, 0xd9, 0xab, 0x06, 0x08, 0xb1, 0xe8, 0xd9, 0xab, 0x06};
    LONGS_EQUAL(sizeof(protobuf), handler.received_payload_size);
    MEMCMP_EQUAL(protobuf, handler.received_payload, sizeof(protobuf));

    // move the clock past the max latency of the second event
    GG_TimerScheduler_SetTime(scheduler, origin + 2500);

    // check that nothing more was emitted
    LONGS_EQUAL(0, handler.call_count);
    LONGS_EQUAL(0, handler.received_events_count);

    // reset
    GG_TimerScheduler_SetTime(scheduler, 0);

    // set an event with a 1 second max latency
    result = GG_CoapEventEmitter_SetEvent(emitter, GG_4CC('e', 'v', 't', '0'), 1000);
    CHECK_TRUE(GG_CoapEventEmitter_EventIsSet(emitter, GG_4CC('e', 'v', 't', '0')))
    LONGS_EQUAL(GG_SUCCESS, result);

    // check that nothing was emitted yet
    LONGS_EQUAL(0, handler.call_count);
    LONGS_EQUAL(0, handler.received_events_count);

    // try to unset a non existing event
    result = GG_CoapEventEmitter_UnsetEvent(emitter, GG_4CC('e', 'v', 't', 'z'));
    LONGS_EQUAL(GG_ERROR_NO_SUCH_ITEM, result);
    CHECK_FALSE(GG_CoapEventEmitter_EventIsSet(emitter, GG_4CC('e', 'v', 't', 'z')));
    CHECK_TRUE(GG_CoapEventEmitter_EventIsSet(emitter, GG_4CC('e', 'v', 't', '0')));

    // unset the event
    result = GG_CoapEventEmitter_UnsetEvent(emitter, GG_4CC('e', 'v', 't', '0'));
    LONGS_EQUAL(GG_SUCCESS, result);
    CHECK_FALSE(GG_CoapEventEmitter_EventIsSet(emitter, GG_4CC('e', 'v', 't', '0')));

    // move the clock past the max latency of the event
    GG_TimerScheduler_SetTime(scheduler, 2000);

    // check that nothing was emitted
    LONGS_EQUAL(0, handler.call_count);
    LONGS_EQUAL(0, handler.received_events_count);

    // reset
    GG_TimerScheduler_SetTime(scheduler, 0);

    // tell the handler to respond with an error
    handler.result_to_return = GG_COAP_MESSAGE_CODE_INTERNAL_SERVER_ERROR;
    handler.call_count = 0;

    // set an event with immediate delivery
    result = GG_CoapEventEmitter_SetEvent(emitter, GG_4CC('e', 'v', 't', '0'), 0);
    LONGS_EQUAL(GG_SUCCESS, result);
    CHECK_TRUE(GG_CoapEventEmitter_EventIsSet(emitter, GG_4CC('e', 'v', 't', '0')))

    // move the clock a bit to flush the async pipe
    GG_TimerScheduler_SetTime(scheduler, GG_TimerScheduler_GetTime(scheduler) + 10);

    // the event should still be set
    CHECK_TRUE(GG_CoapEventEmitter_EventIsSet(emitter, GG_4CC('e', 'v', 't', '0')))
    LONGS_EQUAL(handler.call_count, 1);

    // move the time forward, but less than the resend interval, and check that the event wasn't yet resent
    GG_TimerScheduler_SetTime(scheduler,
                              GG_TimerScheduler_GetTime(scheduler) + GG_COAP_EVENT_EMITTER_DEFAULT_RETRY_DELAY/2);
    LONGS_EQUAL(handler.call_count, 1);

    // move the time forward, past the resend interval, and check that the event was resent
    GG_TimerScheduler_SetTime(scheduler,
                              GG_TimerScheduler_GetTime(scheduler) + GG_COAP_EVENT_EMITTER_DEFAULT_RETRY_DELAY/2 + 10);
    LONGS_EQUAL(handler.call_count, 2);

    // reset
    handler.call_count = 0;
    GG_TimerScheduler_SetTime(scheduler, 0);

    // tell the handler to respond with an error
    handler.result_to_return = GG_COAP_MESSAGE_CODE_INTERNAL_SERVER_ERROR;

    // set an event with immediate delivery
    result = GG_CoapEventEmitter_SetEvent(emitter, GG_4CC('e', 'v', 't', '0'), 0);
    LONGS_EQUAL(GG_SUCCESS, result);
    CHECK_TRUE(GG_CoapEventEmitter_EventIsSet(emitter, GG_4CC('e', 'v', 't', '0')))

    // go back to a normal handler response
    handler.result_to_return = GG_COAP_MESSAGE_CODE_CHANGED;

    // move the time forward a bit, far enough that any in-flight request should
    // be cancellable
    GG_TimerScheduler_SetTime(scheduler, 1500);

    // the event should still be set
    CHECK_TRUE(GG_CoapEventEmitter_EventIsSet(emitter, GG_4CC('e', 'v', 't', '0')))

    // set a second event ready to send now
    result = GG_CoapEventEmitter_SetEvent(emitter, GG_4CC('e', 'v', 't', '1'), 0);
    LONGS_EQUAL(GG_SUCCESS, result);

    // move the clock a bit to flush the async pipe
    GG_TimerScheduler_SetTime(scheduler, GG_TimerScheduler_GetTime(scheduler) + 10);

    // the events should have all been ack'ed
    CHECK_FALSE(GG_CoapEventEmitter_EventIsSet(emitter, GG_4CC('e', 'v', 't', '0')))
    CHECK_FALSE(GG_CoapEventEmitter_EventIsSet(emitter, GG_4CC('e', 'v', 't', '1')))
    handler.call_count = 0;
    handler.received_events_count = 0;

    // reset
    GG_TimerScheduler_SetTime(scheduler, 0);

    // set an event with a 2 seconds max latency
    result = GG_CoapEventEmitter_SetEvent(emitter, GG_4CC('e', 'v', 't', '0'), 2000);
    LONGS_EQUAL(GG_SUCCESS, result);
    CHECK_TRUE(GG_CoapEventEmitter_EventIsSet(emitter, GG_4CC('e', 'v', 't', '0')))

    // set the same event with immediate notification
    result = GG_CoapEventEmitter_SetEvent(emitter, GG_4CC('e', 'v', 't', '0'), 0);
    LONGS_EQUAL(GG_SUCCESS, result);

    // move the clock a bit to flush the async pipe
    GG_TimerScheduler_SetTime(scheduler, GG_TimerScheduler_GetTime(scheduler) + 1);

    // the events should have been delivered
    CHECK_FALSE(GG_CoapEventEmitter_EventIsSet(emitter, GG_4CC('e', 'v', 't', '0')))
    handler.call_count = 0;
    handler.received_events_count = 0;

    // test that a 4.XX response from the server doesn't result in re-transmissions
    GG_TimerScheduler_SetTime(scheduler, 0);
    handler.result_to_return = GG_COAP_MESSAGE_CODE_NOT_FOUND;
    result = GG_CoapEventEmitter_SetEvent(emitter, GG_4CC('e', 'v', 't', '0'), 0);
    LONGS_EQUAL(GG_SUCCESS, result);
    GG_TimerScheduler_SetTime(scheduler, GG_TimerScheduler_GetTime(scheduler) + 10);
    GG_TimerScheduler_SetTime(scheduler, GG_TimerScheduler_GetTime(scheduler) + 10000);
    GG_TimerScheduler_SetTime(scheduler, GG_TimerScheduler_GetTime(scheduler) + 10000);
    LONGS_EQUAL(1, handler.call_count);
    LONGS_EQUAL(1, handler.received_events_count);
    CHECK_FALSE(GG_CoapEventEmitter_EventIsSet(emitter, GG_4CC('e', 'v', 't', '0')))

    // a few edge cases
    GG_CoapEventEmitter_Destroy(NULL);

    result = GG_CoapEventEmitter_SetEvent(emitter, GG_4CC('e', 'v', 't', '0'), 0);
    LONGS_EQUAL(GG_SUCCESS, result);
    result = GG_CoapEventEmitter_SetEvent(emitter, GG_4CC('e', 'v', 't', '1'), 0);
    LONGS_EQUAL(GG_SUCCESS, result);
    result = GG_CoapEventEmitter_SetEvent(emitter, GG_4CC('e', 'v', 't', '2'), 0);
    LONGS_EQUAL(GG_SUCCESS, result);
    result = GG_CoapEventEmitter_SetEvent(emitter, GG_4CC('e', 'v', 't', '3'), 0);
    LONGS_EQUAL(GG_SUCCESS, result);
    result = GG_CoapEventEmitter_SetEvent(emitter, GG_4CC('e', 'v', 't', '4'), 0);
    LONGS_EQUAL(GG_ERROR_OUT_OF_RESOURCES, result);

    // cleanup
    GG_DataSource_SetDataSink(GG_CoapEndpoint_AsDataSource(server_endpoint), NULL);
    GG_DataSource_SetDataSink(GG_AsyncPipe_AsDataSource(pipe), NULL);
    GG_DataSource_SetDataSink(GG_CoapEndpoint_AsDataSource(client_endpoint), NULL);
    GG_CoapEventEmitter_Destroy(emitter);
    GG_CoapEndpoint_Destroy(client_endpoint);
    GG_CoapEndpoint_Destroy(server_endpoint);
    GG_AsyncPipe_Destroy(pipe);
    GG_TimerScheduler_Destroy(scheduler);
}

TEST(GG_COAP_EVENT_EMITTER, Test_BackToBack_Events) {
    GG_Result result;

    // create two endpoints
    GG_TimerScheduler* timer_scheduler1 = NULL;
    GG_TimerScheduler_Create(&timer_scheduler1);
    GG_CoapEndpoint* clientEndpoint;
    GG_CoapEndpoint_Create(timer_scheduler1, NULL, NULL, &clientEndpoint);
    GG_TimerScheduler* timer_scheduler2 = NULL;
    GG_TimerScheduler_Create(&timer_scheduler2);
    GG_CoapEndpoint* serverEndpoint;
    GG_CoapEndpoint_Create(timer_scheduler2, NULL, NULL, &serverEndpoint);

    // connect the two endpoints with async pipes
    GG_AsyncPipe* pipe1 = NULL;
    GG_TimerScheduler* timer_scheduler3 = NULL;
    GG_TimerScheduler_Create(&timer_scheduler3);
    result = GG_AsyncPipe_Create(timer_scheduler3, 1, &pipe1);
    CHECK_EQUAL(GG_SUCCESS, result);
    GG_AsyncPipe* pipe2 = NULL;
    result = GG_AsyncPipe_Create(timer_scheduler2, 1, &pipe2);
    CHECK_EQUAL(GG_SUCCESS, result);
    GG_DataSource_SetDataSink(GG_CoapEndpoint_AsDataSource(clientEndpoint),
                              GG_AsyncPipe_AsDataSink(pipe1));
    GG_DataSource_SetDataSink(GG_AsyncPipe_AsDataSource(pipe1),
                              GG_CoapEndpoint_AsDataSink(serverEndpoint));
    GG_DataSource_SetDataSink(GG_CoapEndpoint_AsDataSource(serverEndpoint),
                              GG_AsyncPipe_AsDataSink(pipe2));
    GG_DataSource_SetDataSink(GG_AsyncPipe_AsDataSource(pipe2),
                              GG_CoapEndpoint_AsDataSink(clientEndpoint));

    // register a test handler
    TestHandler handler;
    GG_SET_INTERFACE(&handler, TestHandler, GG_CoapRequestHandler);
    handler.result_to_return = GG_COAP_MESSAGE_CODE_CHANGED;
    handler.last_message_code_handled = 0;
    handler.drop = false;
    handler.received_events_count = 0;
    handler.decode_error = false;
    handler.call_count = 0;
    result = GG_CoapEndpoint_RegisterRequestHandler(serverEndpoint,
                                                    "foo/bar",
                                                    GG_COAP_REQUEST_HANDLER_FLAG_ALLOW_POST |
                                                    GG_COAP_REQUEST_HANDLER_FLAG_ENABLE_ASYNC,
                                                    GG_CAST(&handler, GG_CoapRequestHandler));

    GG_CoapEventEmitter* emitter = NULL;
    result = GG_CoapEventEmitter_Create(clientEndpoint, "foo/bar", timer_scheduler1, 4, 0, 0, &emitter);
    LONGS_EQUAL(GG_SUCCESS, result);

    // set an event now
    result = GG_CoapEventEmitter_SetEvent(emitter, GG_4CC('e', 'v', 't', '0'), 0);
    uint32_t client_time = 0;
    GG_TimerScheduler_SetTime(timer_scheduler1, ++client_time);

    // Should see that there is a new request in flight and wait for response
    result = GG_CoapEventEmitter_SetEvent(emitter, GG_4CC('e', 'v', 't', '2'), 0);

    // There should only be one event queued up in the pipe
    uint32_t pipe_time = 0;
    GG_TimerScheduler_SetTime(timer_scheduler3, ++pipe_time);

    LONGS_EQUAL(GG_SUCCESS, result);
    LONGS_EQUAL(1, handler.call_count);
    LONGS_EQUAL(1, handler.received_events_count);
    LONGS_EQUAL(GG_4CC('e', 'v', 't', '0'), handler.received_events[0]);
    handler.call_count = 0;
    handler.received_events_count = 0;

    // cleanup
    GG_DataSource_SetDataSink(GG_CoapEndpoint_AsDataSource(clientEndpoint), NULL);
    GG_DataSource_SetDataSink(GG_AsyncPipe_AsDataSource(pipe1), NULL);
    GG_DataSource_SetDataSink(GG_CoapEndpoint_AsDataSource(serverEndpoint), NULL);
    GG_DataSource_SetDataSink(GG_AsyncPipe_AsDataSource(pipe2), NULL);
    GG_CoapEventEmitter_Destroy(emitter);
    GG_CoapEndpoint_Destroy(clientEndpoint);
    GG_CoapEndpoint_Destroy(serverEndpoint);
    GG_AsyncPipe_Destroy(pipe1);
    GG_AsyncPipe_Destroy(pipe2);
    GG_TimerScheduler_Destroy(timer_scheduler1);
    GG_TimerScheduler_Destroy(timer_scheduler2);
    GG_TimerScheduler_Destroy(timer_scheduler3);
}

TEST(GG_COAP_EVENT_EMITTER, Test_CancelEvents) {
    GG_Result result;

    // create two endpoints
    GG_TimerScheduler* timer_scheduler1 = NULL;
    GG_TimerScheduler_Create(&timer_scheduler1);
    GG_CoapEndpoint* clientEndpoint;
    GG_CoapEndpoint_Create(timer_scheduler1, NULL, NULL, &clientEndpoint);
    GG_TimerScheduler* timer_scheduler2 = NULL;
    GG_TimerScheduler_Create(&timer_scheduler2);
    GG_CoapEndpoint* serverEndpoint;
    GG_CoapEndpoint_Create(timer_scheduler2, NULL, NULL, &serverEndpoint);

    // connect the two endpoints with async pipes
    GG_AsyncPipe* pipe1 = NULL;
    GG_TimerScheduler* timer_scheduler3 = NULL;
    GG_TimerScheduler_Create(&timer_scheduler3);
    result = GG_AsyncPipe_Create(timer_scheduler3, 1, &pipe1);
    CHECK_EQUAL(GG_SUCCESS, result);
    GG_AsyncPipe* pipe2 = NULL;
    result = GG_AsyncPipe_Create(timer_scheduler2, 1, &pipe2);
    CHECK_EQUAL(GG_SUCCESS, result);
    GG_DataSource_SetDataSink(GG_CoapEndpoint_AsDataSource(clientEndpoint),
                              GG_AsyncPipe_AsDataSink(pipe1));
    GG_DataSource_SetDataSink(GG_AsyncPipe_AsDataSource(pipe1),
                              GG_CoapEndpoint_AsDataSink(serverEndpoint));
    GG_DataSource_SetDataSink(GG_CoapEndpoint_AsDataSource(serverEndpoint),
                              GG_AsyncPipe_AsDataSink(pipe2));
    GG_DataSource_SetDataSink(GG_AsyncPipe_AsDataSource(pipe2),
                              GG_CoapEndpoint_AsDataSink(clientEndpoint));

    // register a test handler
    TestHandler handler;
    GG_SET_INTERFACE(&handler, TestHandler, GG_CoapRequestHandler);
    handler.result_to_return = GG_COAP_MESSAGE_CODE_CHANGED;
    handler.last_message_code_handled = 0;
    handler.drop = false;
    handler.received_events_count = 0;
    handler.decode_error = false;
    handler.call_count = 0;
    result = GG_CoapEndpoint_RegisterRequestHandler(serverEndpoint,
                                                    "foo/bar",
                                                    GG_COAP_REQUEST_HANDLER_FLAG_ALLOW_POST |
                                                    GG_COAP_REQUEST_HANDLER_FLAG_ENABLE_ASYNC,
                                                    GG_CAST(&handler, GG_CoapRequestHandler));

    GG_CoapEventEmitter* emitter = NULL;
    result = GG_CoapEventEmitter_Create(clientEndpoint, "foo/bar", timer_scheduler1, 4, 0, 0, &emitter);
    LONGS_EQUAL(GG_SUCCESS, result);

    // set an event now
    result = GG_CoapEventEmitter_SetEvent(emitter, GG_4CC('e', 'v', 't', '0'), 0);

    uint32_t client_time = GG_COAP_EVENT_EMITTER_DEFAULT_MIN_REQUEST_AGE;
    GG_TimerScheduler_SetTime(timer_scheduler1, ++client_time);

    // Should see that request in flight is old and add a new one
    result = GG_CoapEventEmitter_SetEvent(emitter, GG_4CC('e', 'v', 't', '2'), 0);

    // There should two events queued up in the pipe since we should have cancelled one
    uint32_t pipe_time = 0;
    GG_TimerScheduler_SetTime(timer_scheduler3, ++pipe_time);

    LONGS_EQUAL(GG_SUCCESS, result);
    LONGS_EQUAL(2, handler.call_count);
    LONGS_EQUAL(2, handler.received_events_count);
    LONGS_EQUAL(GG_4CC('e', 'v', 't', '0'), handler.received_events[0]);
    handler.call_count = 0;
    handler.received_events_count = 0;

    // cleanup
    GG_DataSource_SetDataSink(GG_CoapEndpoint_AsDataSource(clientEndpoint), NULL);
    GG_DataSource_SetDataSink(GG_AsyncPipe_AsDataSource(pipe1), NULL);
    GG_DataSource_SetDataSink(GG_CoapEndpoint_AsDataSource(serverEndpoint), NULL);
    GG_DataSource_SetDataSink(GG_AsyncPipe_AsDataSource(pipe2), NULL);
    GG_CoapEventEmitter_Destroy(emitter);
    GG_CoapEndpoint_Destroy(clientEndpoint);
    GG_CoapEndpoint_Destroy(serverEndpoint);
    GG_AsyncPipe_Destroy(pipe1);
    GG_AsyncPipe_Destroy(pipe2);
    GG_TimerScheduler_Destroy(timer_scheduler1);
    GG_TimerScheduler_Destroy(timer_scheduler2);
    GG_TimerScheduler_Destroy(timer_scheduler3);
}

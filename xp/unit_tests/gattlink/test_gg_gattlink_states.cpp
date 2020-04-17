// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

#include "CppUTest/TestHarness.h"

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdio.h>
#include <string.h>

#include "xp/common/gg_port.h"
#include "xp/common/gg_timer.h"
#include "xp/common/gg_types.h"
#include "xp/common/gg_io.h"
#include "xp/common/gg_buffer.h"
#include "xp/common/gg_utils.h"
#include "xp/gattlink/gg_gattlink.h"
#include "xp/utils/gg_async_pipe.h"

//----------------------------------------------------------------------
static uint32_t trivial_rand() {
    static uint16_t lfsr = 0xACE1;
    static uint16_t bit;

    bit  = ((lfsr >> 0) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 5) ) & 1;
    lfsr = (uint16_t)((lfsr >> 1) | (bit << 15));

    return lfsr;
}

//----------------------------------------------------------------------
//  Gattlink Client
//----------------------------------------------------------------------
typedef enum {
    STATE_INIT,
    STATE_SESSION_READY,
    STATE_SESSION_RESET
} GattlinkClientState;

typedef struct {
    GG_IMPLEMENTS(GG_GattlinkClient);
    GG_IMPLEMENTS(GG_DataSink);
    GG_IMPLEMENTS(GG_TimerListener);

    const char*          name;
    GG_GattlinkProtocol* protocol;
    GG_AsyncPipe*        async_pipe;
    GattlinkClientState  state;
    size_t               max_transport_packet_size;
    GG_Buffer*           pending_packet;
    GG_DataSinkListener* listener;
    GG_DataSink*         send_sink;
    size_t               bytes_sent;
    size_t               bytes_received;
    size_t               send_payload_size;
    size_t               expected_receive_payload_size;
    size_t               packet_count;
    size_t               drop_count;
    const uint32_t*      packet_policies;      // a policy is: 0-100 -> probability of dropping (100 = drop)
                                               // 100 < x < 1000: delay by x - 100
    size_t               packet_policy_count;
    bool                 repeat_packet_policies;
    size_t               max_stall;
    GG_Timer*            timer;
} GattlinkClient;

//----------------------------------------------------------------------
static void
GattlinkClient_NotifySessionReady(GG_GattlinkClient* _self)
{
    GattlinkClient* self = GG_SELF(GattlinkClient, GG_GattlinkClient);

    // we can only transition to STATE_SESSION_READY from STATE_INIT or STATE_SESSION_RESET
    CHECK_TRUE(self->state == STATE_INIT || self->state == STATE_SESSION_RESET);
    self->state = STATE_SESSION_READY;

    GG_GattlinkProtocol_NotifyOutgoingDataAvailable(self->protocol);
}

//----------------------------------------------------------------------
static void
GattlinkClient_NotifySessionReset(GG_GattlinkClient* _self)
{
    GattlinkClient* self = GG_SELF(GattlinkClient, GG_GattlinkClient);

    // we can only transition to STATE_SESSION_RESET from STATE_SESSION_READY
    CHECK_TRUE(self->state == STATE_SESSION_READY);
    self->state = STATE_SESSION_RESET;
}

//----------------------------------------------------------------------
static void
GattlinkClient_NotifySessionStalled(GG_GattlinkClient* _self, uint32_t stalled_time)
{
    GattlinkClient* self = GG_SELF(GattlinkClient, GG_GattlinkClient);

    if (stalled_time > self->max_stall) {
        self->max_stall = stalled_time;
    }
}

//----------------------------------------------------------------------
static GG_Result
GattlinkClient_SendRawData(GG_GattlinkClient* _self, const void *tx_raw_data, size_t tx_raw_data_len)
{
    GattlinkClient* self = GG_SELF(GattlinkClient, GG_GattlinkClient);

    if (!self->send_sink) {
        return GG_SUCCESS; // no sink, drop
    }

    CHECK_TRUE(tx_raw_data_len <= self->max_transport_packet_size);
    GG_DynamicBuffer* buffer;
    GG_Result result = GG_DynamicBuffer_Create(tx_raw_data_len, &buffer);
    LONGS_EQUAL(GG_SUCCESS, result);
    GG_DynamicBuffer_SetData(buffer, (const uint8_t*)tx_raw_data, tx_raw_data_len);

    result = GG_DataSink_PutData(self->send_sink,
                                 GG_DynamicBuffer_AsBuffer(buffer),
                                 NULL);
    LONGS_EQUAL(GG_SUCCESS, result);

    GG_DynamicBuffer_Release(buffer);

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
static size_t
GattlinkClient_GetTransportMaxPacketSize(GG_GattlinkClient* _self)
{
    GattlinkClient* self = GG_SELF(GattlinkClient, GG_GattlinkClient);

    return self->max_transport_packet_size;
}

//----------------------------------------------------------------------
static size_t
GattlinkClient_GetOutgoingDataAvailable(GG_GattlinkClient* _self)
{
    GattlinkClient* self = GG_SELF(GattlinkClient, GG_GattlinkClient);

    CHECK_TRUE(self->bytes_sent <= self->send_payload_size);
    return self->send_payload_size - self->bytes_sent;
}

//----------------------------------------------------------------------
static int
GattlinkClient_GetOutgoingData(GG_GattlinkClient* _self, size_t offset, void* buffer, size_t size)
{
    GattlinkClient* self = GG_SELF(GattlinkClient, GG_GattlinkClient);

    uint8_t* data = (uint8_t*)buffer;
    for (size_t x = 0; x < size; x++) {
        data[x] = (uint8_t)(self->bytes_sent + offset + x);
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
static void
GattlinkClient_ConsumeOutgoingData(GG_GattlinkClient* _self, size_t num_bytes)
{
    GattlinkClient* self = GG_SELF(GattlinkClient, GG_GattlinkClient);

    self->bytes_sent += num_bytes;
    CHECK_TRUE(self->bytes_sent <= self->send_payload_size);
}

//----------------------------------------------------------------------
static void
GattlinkClient_NotifyIncomingDataAvailable(GG_GattlinkClient* _self)
{
    GattlinkClient* self = GG_SELF(GattlinkClient, GG_GattlinkClient);

    size_t available = GG_GattlinkProtocol_GetIncomingDataAvailable(self->protocol);

    bool content_valid = true;
    uint8_t* buffer = new uint8_t[available];
    GG_GattlinkProtocol_GetIncomingData(self->protocol, 0, buffer, available);
    for (size_t x = 0; x < available; x++) {
        if (buffer[x] != (uint8_t)(self->bytes_received + x)) {
            content_valid = false;
            break;
        }
    }
    delete[] buffer;
    CHECK_TRUE(content_valid);

    GG_GattlinkProtocol_ConsumeIncomingData(self->protocol, available);
    self->bytes_received += available;
    CHECK_TRUE(self->bytes_received <= self->expected_receive_payload_size);

    GG_COMPILER_UNUSED(_self);
}

//----------------------------------------------------------------------
static GG_Result
GattlinkClient_PutData(GG_DataSink* _self, GG_Buffer* data, const GG_BufferMetadata* metadata)
{
    GattlinkClient* self = GG_SELF(GattlinkClient, GG_DataSink);
    GG_COMPILER_UNUSED(metadata);

    // check that we don't already have a pending packet
    if (self->pending_packet) {
        return GG_ERROR_WOULD_BLOCK;
    }

    // check if we need to drop this packet
    uint32_t packet_policy = 0;
    if (self->packet_policies && self->packet_policy_count) {
        size_t index = self->packet_count % self->packet_policy_count;
        if (self->packet_count > self->packet_policy_count) {
            if (self->repeat_packet_policies) {
                packet_policy = self->packet_policies[index];
            }
        } else {
            packet_policy = self->packet_policies[index];
        }
    }
    ++self->packet_count;

    if (packet_policy <= 100) {
        // probability of passing
        bool drop = ((trivial_rand() % 100) < packet_policy);

        if (drop) {
            ++self->drop_count;
        } else {
            // deliver the packet immediately
            GG_Result result = GG_GattlinkProtocol_HandleIncomingRawData(self->protocol,
                                                                         GG_Buffer_GetData(data),
                                                                         GG_Buffer_GetDataSize(data));
            if (self->state == STATE_SESSION_READY) {
                // during a session, only expect GG_SUCCESS or GG_ERROR_GATTLINK_UNEXPECTED_PSN
                if (result != GG_SUCCESS && result != GG_ERROR_GATTLINK_UNEXPECTED_PSN) {
                    LONGS_EQUAL(GG_SUCCESS, result);
                }
            }
        }
    } else if (packet_policy < 1000) {
        // deliver the packet later
        self->pending_packet = GG_Buffer_Retain(data);
        GG_Timer_Schedule(self->timer, GG_CAST(self, GG_TimerListener), packet_policy - 100);
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
static GG_Result
GattlinkClient_SetListener(GG_DataSink* _self, GG_DataSinkListener* listener)
{
    GattlinkClient* self = GG_SELF(GattlinkClient, GG_DataSink);
    self->listener = listener;

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
static void
GattlinkClient_OnTimerFired(GG_TimerListener* _self, GG_Timer* timer, uint32_t time_elapsed)
{
    GattlinkClient* self = GG_SELF(GattlinkClient, GG_TimerListener);
    GG_COMPILER_UNUSED(timer);
    GG_COMPILER_UNUSED(time_elapsed);

    if (self->pending_packet) {
        GG_Result result = GG_GattlinkProtocol_HandleIncomingRawData(self->protocol,
                                                                     GG_Buffer_GetData(self->pending_packet),
                                                                     GG_Buffer_GetDataSize(self->pending_packet));
        LONGS_EQUAL(GG_SUCCESS, result);
        GG_Buffer_Release(self->pending_packet);
        self->pending_packet = NULL;

        if (self->listener) {
            GG_DataSinkListener_OnCanPut(self->listener);
        }
    }
}

//----------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(GattlinkClient, GG_GattlinkClient) {
    .GetOutgoingDataAvailable    = GattlinkClient_GetOutgoingDataAvailable,
    .GetOutgoingData             = GattlinkClient_GetOutgoingData,
    .ConsumeOutgoingData         = GattlinkClient_ConsumeOutgoingData,
    .NotifyIncomingDataAvailable = GattlinkClient_NotifyIncomingDataAvailable,
    .GetTransportMaxPacketSize   = GattlinkClient_GetTransportMaxPacketSize,
    .SendRawData                 = GattlinkClient_SendRawData,
    .NotifySessionReady          = GattlinkClient_NotifySessionReady,
    .NotifySessionReset          = GattlinkClient_NotifySessionReset,
    .NotifySessionStalled        = GattlinkClient_NotifySessionStalled
};

//----------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(GattlinkClient, GG_DataSink) {
    .PutData     = GattlinkClient_PutData,
    .SetListener = GattlinkClient_SetListener
};

//----------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(GattlinkClient, GG_TimerListener) {
    .OnTimerFired = GattlinkClient_OnTimerFired
};

//----------------------------------------------------------------------
static GG_Result
GattlinkClient_Init(GattlinkClient*    self,
                    const char*        name,
                    GG_TimerScheduler* scheduler,
                    const uint32_t*    packet_policies,
                    size_t             packet_policy_count,
                    bool               repeat_packet_policies)
{
    memset(self, 0, sizeof(*self));
    self->name = name;

    // create an async pipe to communicate between the two clients
    GG_Result result = GG_AsyncPipe_Create(scheduler, 8, &self->async_pipe);
    LONGS_EQUAL(GG_SUCCESS, result);

    // create a gattlink protocol
    GG_GattlinkSessionConfig config = {
        4, 4
    };
    result = GG_GattlinkProtocol_Create(GG_CAST(self, GG_GattlinkClient),
                                        &config,
                                        scheduler,
                                        &self->protocol);
    LONGS_EQUAL(GG_SUCCESS, result);

    // init the state
    self->state = STATE_INIT;
    self->max_transport_packet_size   = 100;
    self->packet_policies             = packet_policies;
    self->packet_policy_count         = packet_policy_count;
    self->repeat_packet_policies      = repeat_packet_policies;
    result = GG_TimerScheduler_CreateTimer(scheduler, &self->timer);
    LONGS_EQUAL(GG_SUCCESS, result);

    // setup interfaces
    GG_SET_INTERFACE(self, GattlinkClient, GG_GattlinkClient);
    GG_SET_INTERFACE(self, GattlinkClient, GG_DataSink);
    GG_SET_INTERFACE(self, GattlinkClient, GG_TimerListener);

    // register to get the data from the async pipe
    GG_DataSource_SetDataSink(GG_AsyncPipe_AsDataSource(self->async_pipe), GG_CAST(self, GG_DataSink));

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
static void
GattlinkClient_Deinit(GattlinkClient* self)
{
    GG_GattlinkProtocol_Destroy(self->protocol);
    GG_AsyncPipe_Destroy(self->async_pipe);
    GG_Timer_Destroy(self->timer);
    if (self->pending_packet) {
        GG_Buffer_Release(self->pending_packet);
    }
}

TEST_GROUP(GATTLINK)
{
    void setup(void) {
    }

    void teardown(void) {
    }
};

TEST(GATTLINK, Test_GattlinkDropNone)
{
    GattlinkClient client1;
    GattlinkClient client2;
    GG_Result      result;

    // create a timer scheduler to drive the timing
    GG_TimerScheduler* scheduler;
    result = GG_TimerScheduler_Create(&scheduler);
    LONGS_EQUAL(GG_SUCCESS, result);

    result = GattlinkClient_Init(&client1, "client1", scheduler, NULL, 0, false);
    LONGS_EQUAL(GG_SUCCESS, result);
    result = GattlinkClient_Init(&client2, "client2", scheduler, NULL, 0, false);
    LONGS_EQUAL(GG_SUCCESS, result);

    // connect the two clients
    client1.send_sink = GG_AsyncPipe_AsDataSink(client2.async_pipe);
    client2.send_sink = GG_AsyncPipe_AsDataSink(client1.async_pipe);

    result = GG_GattlinkProtocol_Start(client1.protocol);
    LONGS_EQUAL(GG_SUCCESS, result);
    result = GG_GattlinkProtocol_Start(client2.protocol);
    LONGS_EQUAL(GG_SUCCESS, result);

    for (unsigned int i = 0; i < 10000; i++) {
        GG_TimerScheduler_SetTime(scheduler, i);
    }

    LONGS_EQUAL(STATE_SESSION_READY, client1.state);
    LONGS_EQUAL(STATE_SESSION_READY, client2.state);

    GattlinkClient_Deinit(&client2);
    GattlinkClient_Deinit(&client1);
    GG_TimerScheduler_Destroy(scheduler);
}

TEST(GATTLINK, Test_GattlinkDropAll)
{
    GattlinkClient client1;
    GattlinkClient client2;
    GG_Result      result;

    // create a timer scheduler to drive the timing
    GG_TimerScheduler* scheduler;
    result = GG_TimerScheduler_Create(&scheduler);
    LONGS_EQUAL(GG_SUCCESS, result);

    uint32_t drop = 100;
    result = GattlinkClient_Init(&client1, "client1", scheduler, &drop, 1, true);
    LONGS_EQUAL(GG_SUCCESS, result);
    result = GattlinkClient_Init(&client2, "client2", scheduler, &drop, 1, true);
    LONGS_EQUAL(GG_SUCCESS, result);

    // connect the two clients
    client1.send_sink = GG_AsyncPipe_AsDataSink(client2.async_pipe);
    client2.send_sink = GG_AsyncPipe_AsDataSink(client1.async_pipe);

    result = GG_GattlinkProtocol_Start(client1.protocol);
    LONGS_EQUAL(GG_SUCCESS, result);
    result = GG_GattlinkProtocol_Start(client2.protocol);
    LONGS_EQUAL(GG_SUCCESS, result);

    for (unsigned int i = 0; i < 1000; i++) {
        GG_TimerScheduler_SetTime(scheduler, i);
    }

    LONGS_EQUAL(STATE_INIT, client1.state);
    LONGS_EQUAL(STATE_INIT, client2.state);

    GattlinkClient_Deinit(&client2);
    GattlinkClient_Deinit(&client1);
    GG_TimerScheduler_Destroy(scheduler);
}

TEST(GATTLINK, Test_GattlinkDelays)
{
    GattlinkClient client1;
    GattlinkClient client2;
    GG_Result      result;

    // create a timer scheduler to drive the timing
    GG_TimerScheduler* scheduler;
    result = GG_TimerScheduler_Create(&scheduler);
    LONGS_EQUAL(GG_SUCCESS, result);

    uint32_t delays1[2] = {200};
    result = GattlinkClient_Init(&client1, "client1", scheduler, delays1, 1, true);
    LONGS_EQUAL(GG_SUCCESS, result);
    uint32_t delays2[3] = {300};
    result = GattlinkClient_Init(&client2, "client2", scheduler, delays2, 1, true);
    LONGS_EQUAL(GG_SUCCESS, result);

    // connect the two clients
    client1.send_sink = GG_AsyncPipe_AsDataSink(client2.async_pipe);
    client2.send_sink = GG_AsyncPipe_AsDataSink(client1.async_pipe);

    result = GG_GattlinkProtocol_Start(client1.protocol);
    LONGS_EQUAL(GG_SUCCESS, result);
    result = GG_GattlinkProtocol_Start(client2.protocol);
    LONGS_EQUAL(GG_SUCCESS, result);

    for (unsigned int i = 0; i < 10000; i++) {
        GG_TimerScheduler_SetTime(scheduler, i);
    }

    LONGS_EQUAL(STATE_SESSION_READY, client1.state);
    LONGS_EQUAL(STATE_SESSION_READY, client2.state);

    GattlinkClient_Deinit(&client2);
    GattlinkClient_Deinit(&client1);
    GG_TimerScheduler_Destroy(scheduler);
}

TEST(GATTLINK, Test_GattlinkDropSome)
{
    GattlinkClient client1;
    GattlinkClient client2;
    GG_Result      result;

    // create a timer scheduler to drive the timing
    GG_TimerScheduler* scheduler;
    result = GG_TimerScheduler_Create(&scheduler);
    LONGS_EQUAL(GG_SUCCESS, result);

    uint32_t policies1[3] = {102, 100, 105};
    result = GattlinkClient_Init(&client1, "client1", scheduler, policies1, GG_ARRAY_SIZE(policies1), true);
    LONGS_EQUAL(GG_SUCCESS, result);
    uint32_t policies2[4] = {100, 102, 103, 104};
    result = GattlinkClient_Init(&client2, "client2", scheduler, policies2, GG_ARRAY_SIZE(policies2), true);
    LONGS_EQUAL(GG_SUCCESS, result);

    // connect the two clients
    client1.send_sink = GG_AsyncPipe_AsDataSink(client2.async_pipe);
    client2.send_sink = GG_AsyncPipe_AsDataSink(client1.async_pipe);

    result = GG_GattlinkProtocol_Start(client1.protocol);
    LONGS_EQUAL(GG_SUCCESS, result);
    result = GG_GattlinkProtocol_Start(client2.protocol);
    LONGS_EQUAL(GG_SUCCESS, result);

    for (unsigned int i = 0; i < 10000; i++) {
        GG_TimerScheduler_SetTime(scheduler, i);
    }

    LONGS_EQUAL(STATE_SESSION_READY, client1.state);
    LONGS_EQUAL(STATE_SESSION_READY, client2.state);

    GattlinkClient_Deinit(&client2);
    GattlinkClient_Deinit(&client1);
    GG_TimerScheduler_Destroy(scheduler);
}

TEST(GATTLINK, Test_GattlinkRandom50NoData)
{
    GattlinkClient client1;
    GattlinkClient client2;
    GG_Result      result;

    for (unsigned int n = 0; n < 100; n++) {
        // create a timer scheduler to drive the timing
        GG_TimerScheduler* scheduler;
        result = GG_TimerScheduler_Create(&scheduler);
        LONGS_EQUAL(GG_SUCCESS, result);

        uint32_t policies1[1] = {50}; // 50% chance of dropping
        result = GattlinkClient_Init(&client1, "client1", scheduler, policies1, GG_ARRAY_SIZE(policies1), true);
        LONGS_EQUAL(GG_SUCCESS, result);
        uint32_t policies2[1] = {50}; // 50% chance of dropping
        result = GattlinkClient_Init(&client2, "client2", scheduler, policies2, GG_ARRAY_SIZE(policies2), true);
        LONGS_EQUAL(GG_SUCCESS, result);

        // connect the two clients
        client1.send_sink = GG_AsyncPipe_AsDataSink(client2.async_pipe);
        client2.send_sink = GG_AsyncPipe_AsDataSink(client1.async_pipe);

        result = GG_GattlinkProtocol_Start(client1.protocol);
        LONGS_EQUAL(GG_SUCCESS, result);
        result = GG_GattlinkProtocol_Start(client2.protocol);
        LONGS_EQUAL(GG_SUCCESS, result);

        for (unsigned int i = 0; i < 10000; i++) {
            GG_TimerScheduler_SetTime(scheduler, i * 10);
        }

        LONGS_EQUAL(STATE_SESSION_READY, client1.state);
        LONGS_EQUAL(STATE_SESSION_READY, client2.state);
        GattlinkClient_Deinit(&client2);
        GattlinkClient_Deinit(&client1);
        GG_TimerScheduler_Destroy(scheduler);
    }
}

TEST(GATTLINK, Test_GattlinkRandom50WithData)
{
    GattlinkClient client1;
    GattlinkClient client2;
    GG_Result      result;

    // create a timer scheduler to drive the timing
    GG_TimerScheduler* scheduler;
    result = GG_TimerScheduler_Create(&scheduler);
    LONGS_EQUAL(GG_SUCCESS, result);

    size_t payload_size = 10000;
    uint32_t policies1[1] = {50}; // 50% chance of dropping
    result = GattlinkClient_Init(&client1, "client1", scheduler, policies1, GG_ARRAY_SIZE(policies1), true);
    client1.send_payload_size = payload_size;
    client1.expected_receive_payload_size = payload_size;
    LONGS_EQUAL(GG_SUCCESS, result);
    uint32_t policies2[1] = {50}; // 50% chance of dropping
    result = GattlinkClient_Init(&client2, "client2", scheduler, policies2, GG_ARRAY_SIZE(policies2), true);
    client2.send_payload_size = payload_size;
    client2.expected_receive_payload_size = payload_size;
    LONGS_EQUAL(GG_SUCCESS, result);

    // connect the two clients
    client1.send_sink = GG_AsyncPipe_AsDataSink(client2.async_pipe);
    client2.send_sink = GG_AsyncPipe_AsDataSink(client1.async_pipe);

    result = GG_GattlinkProtocol_Start(client1.protocol);
    LONGS_EQUAL(GG_SUCCESS, result);
    result = GG_GattlinkProtocol_Start(client2.protocol);
    LONGS_EQUAL(GG_SUCCESS, result);

    for (unsigned int i = 0; i < 1000000; i++) {
        GG_TimerScheduler_SetTime(scheduler, i);
    }

    LONGS_EQUAL(STATE_SESSION_READY, client1.state);
    LONGS_EQUAL(STATE_SESSION_READY, client2.state);
    LONGS_EQUAL(payload_size, client1.bytes_received);
    LONGS_EQUAL(payload_size, client2.bytes_received);
    LONGS_EQUAL(payload_size, client1.bytes_sent);
    LONGS_EQUAL(payload_size, client2.bytes_sent);

    GattlinkClient_Deinit(&client2);
    GattlinkClient_Deinit(&client1);
    GG_TimerScheduler_Destroy(scheduler);
}

TEST(GATTLINK, Test_GattlinkStall)
{
    GattlinkClient client1;
    GattlinkClient client2;
    GG_Result      result;

    // create a timer scheduler to drive the timing
    GG_TimerScheduler* scheduler;
    result = GG_TimerScheduler_Create(&scheduler);
    LONGS_EQUAL(GG_SUCCESS, result);

    size_t payload_size = 10000;
    uint32_t policies1[100];
    for (unsigned int i = 0; i < 4; i++) {
        policies1[i] = 0; // no drop
    }
    for (unsigned int i = 4; i < GG_ARRAY_SIZE(policies1); i++) {
        policies1[i] = 100; // always drop
    }
    result = GattlinkClient_Init(&client1, "client1", scheduler, policies1, GG_ARRAY_SIZE(policies1), true);
    client1.send_payload_size = payload_size;
    client1.expected_receive_payload_size = payload_size;
    LONGS_EQUAL(GG_SUCCESS, result);
    result = GattlinkClient_Init(&client2, "client2", scheduler, policies1, GG_ARRAY_SIZE(policies1), true);
    client2.send_payload_size = payload_size;
    client2.expected_receive_payload_size = payload_size;
    LONGS_EQUAL(GG_SUCCESS, result);

    // connect the two clients
    client1.send_sink = GG_AsyncPipe_AsDataSink(client2.async_pipe);
    client2.send_sink = GG_AsyncPipe_AsDataSink(client1.async_pipe);

    result = GG_GattlinkProtocol_Start(client1.protocol);
    LONGS_EQUAL(GG_SUCCESS, result);
    result = GG_GattlinkProtocol_Start(client2.protocol);
    LONGS_EQUAL(GG_SUCCESS, result);

    for (unsigned int i = 0; i < 10000; i++) {
        GG_TimerScheduler_SetTime(scheduler, i * 10);
    }

    LONGS_EQUAL(STATE_SESSION_READY, client1.state);
    LONGS_EQUAL(STATE_SESSION_READY, client2.state);
    CHECK_TRUE(client1.max_stall > 0);
    CHECK_TRUE(client2.max_stall > 0);

    GattlinkClient_Deinit(&client2);
    GattlinkClient_Deinit(&client1);
    GG_TimerScheduler_Destroy(scheduler);
}

TEST(GATTLINK, Test_GattlinkPacketLossSingleDirection)
{
    GattlinkClient client1;
    GattlinkClient client2;
    GG_Result      result;

    // create a timer scheduler to drive the timing
    GG_TimerScheduler* scheduler;
    result = GG_TimerScheduler_Create(&scheduler);
    LONGS_EQUAL(GG_SUCCESS, result);

    size_t payload_size = 1000;
    uint32_t policies1[1] = { 0 }; // no drop
    result = GattlinkClient_Init(&client1, "client1", scheduler, policies1, GG_ARRAY_SIZE(policies1), true);
    client1.send_payload_size = payload_size;
    client1.expected_receive_payload_size = 0;
    LONGS_EQUAL(GG_SUCCESS, result);
    uint32_t policies2[1] = { 30 }; // 30% drop
    result = GattlinkClient_Init(&client2, "client2", scheduler, policies2, GG_ARRAY_SIZE(policies2), true);
    client2.send_payload_size = 0;
    client2.expected_receive_payload_size = payload_size;
    LONGS_EQUAL(GG_SUCCESS, result);

    // connect the two clients
    client1.send_sink = GG_AsyncPipe_AsDataSink(client2.async_pipe);
    client2.send_sink = GG_AsyncPipe_AsDataSink(client1.async_pipe);

    result = GG_GattlinkProtocol_Start(client1.protocol);
    LONGS_EQUAL(GG_SUCCESS, result);
    result = GG_GattlinkProtocol_Start(client2.protocol);
    LONGS_EQUAL(GG_SUCCESS, result);

    for (unsigned int i = 0; i < 10000; i++) {
        GG_TimerScheduler_SetTime(scheduler, i * 10);
    }

    LONGS_EQUAL(STATE_SESSION_READY, client1.state);
    LONGS_EQUAL(STATE_SESSION_READY, client2.state);
    LONGS_EQUAL(0, client1.bytes_received);
    LONGS_EQUAL(payload_size, client2.bytes_received);
    LONGS_EQUAL(payload_size, client1.bytes_sent);
    LONGS_EQUAL(0, client2.bytes_sent);

    GattlinkClient_Deinit(&client2);
    GattlinkClient_Deinit(&client1);
    GG_TimerScheduler_Destroy(scheduler);
}

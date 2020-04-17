// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/

#include <cstring>

#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

#include "CppUTest/MemoryLeakDetectorNewMacros.h"

#include "xp/module/gg_module.h"
#include "xp/common/gg_lists.h"
#include "xp/common/gg_port.h"
#include "xp/common/gg_logging.h"
#include "xp/common/gg_buffer.h"
#include "xp/common/gg_utils.h"
#include "xp/utils/gg_async_pipe.h"
#include "xp/utils/gg_memory_data_source.h"
#include "xp/utils/gg_memory_data_sink.h"
#include "xp/nip/gg_nip.h"
#include "xp/gattlink/gg_gattlink_generic_client.h"
#include "xp/protocols/gg_ipv4_protocol.h"

/*----------------------------------------------------------------------
|   logging
+---------------------------------------------------------------------*/
GG_SET_LOCAL_LOGGER("test.gg.xp.gattlink.generic-client")

/*----------------------------------------------------------------------
|   globals
+---------------------------------------------------------------------*/
static GG_TimerScheduler* TimerScheduler;
static uint32_t TimerSchedulerNow;

//----------------------------------------------------------------------
TEST_GROUP(GG_GENERIC_GATTLINK_CLIENT)
{
    void setup(void) {
        GG_Result result = GG_TimerScheduler_Create(&TimerScheduler);
        CHECK_EQUAL(GG_SUCCESS, result);
    }

    void teardown(void) {
        GG_TimerScheduler_Destroy(TimerScheduler);
        TimerScheduler = NULL;
    }
};

//----------------------------------------------------------------------
typedef struct {
    GG_IMPLEMENTS(GG_FrameAssembler);

    uint8_t buffer[1024];
} TestFrameAssembler;

//----------------------------------------------------------------------
static void
TestFrameAssembler_GetFeedBuffer(GG_FrameAssembler* _self, uint8_t** buffer, size_t* buffer_size)
{
    TestFrameAssembler* self = GG_SELF(TestFrameAssembler, GG_FrameAssembler);

    *buffer      = self->buffer;
    *buffer_size = sizeof(self->buffer);
}

//----------------------------------------------------------------------
static GG_Result
TestFrameAssembler_Feed(GG_FrameAssembler* _self, size_t* data_size, GG_Buffer** frame)
{
    TestFrameAssembler* self = GG_SELF(TestFrameAssembler, GG_FrameAssembler);

    GG_DynamicBuffer* buffer = NULL;
    GG_DynamicBuffer_Create(*data_size, &buffer);
    GG_DynamicBuffer_SetData(buffer, &self->buffer[0], *data_size);
    *frame = GG_DynamicBuffer_AsBuffer(buffer);

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
static void
TestFrameAssembler_Reset(GG_FrameAssembler* _self)
{
    GG_COMPILER_UNUSED(_self);
}

//----------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(TestFrameAssembler, GG_FrameAssembler) {
    .GetFeedBuffer = TestFrameAssembler_GetFeedBuffer,
    .Feed          = TestFrameAssembler_Feed,
    .Reset         = TestFrameAssembler_Reset
};

//----------------------------------------------------------------------
typedef struct {
    GG_IMPLEMENTS(GG_DataSink);

    GG_DataSink* sink;
    size_t       packet_count;
    size_t       byte_count;
    size_t       max_packet_size;
} CountingPassthroughSink;

static GG_Result
CountingPassthroughSink_PutData(GG_DataSink* _self, GG_Buffer* buffer, const GG_BufferMetadata* metadata)
{
    CountingPassthroughSink* self = GG_SELF(CountingPassthroughSink, GG_DataSink);
    GG_Result result = GG_DataSink_PutData(self->sink, buffer, metadata);
    if (GG_SUCCEEDED(result)) {
        size_t packet_size = GG_Buffer_GetDataSize(buffer);
        ++self->packet_count;
        self->byte_count += packet_size;
        self->max_packet_size = GG_MAX(self->max_packet_size, packet_size);
    }

    return result;
}

static GG_Result
CountingPassthroughSink_SetListener(GG_DataSink* _self, GG_DataSinkListener* listener)
{
    CountingPassthroughSink* self = GG_SELF(CountingPassthroughSink, GG_DataSink);
    return GG_DataSink_SetListener(self->sink, listener);
}

GG_IMPLEMENT_INTERFACE(CountingPassthroughSink, GG_DataSink) {
    .PutData     = CountingPassthroughSink_PutData,
    .SetListener = CountingPassthroughSink_SetListener
};

static void advance_timer_time(uint32_t value, uint32_t increment) {
    for (uint32_t now = TimerSchedulerNow; now < TimerSchedulerNow + value; now += increment) {
        GG_TimerScheduler_SetTime(TimerScheduler, now);
    }
}

// ----------------------------------------------------------------------
TEST(GG_GENERIC_GATTLINK_CLIENT, Test_GattlinkGenericClient_Basics) {
    GG_Result result;

    // create a frame serializer
    GG_Ipv4FrameSerializer* frame_serializer;
    result = GG_Ipv4FrameSerializer_Create(NULL, &frame_serializer);
    LONGS_EQUAL(GG_SUCCESS, result);

    // create a first frame assembler
    TestFrameAssembler frame_assembler_a;
    GG_SET_INTERFACE(&frame_assembler_a, TestFrameAssembler, GG_FrameAssembler);

    // create a first gattlink client
    GG_GattlinkGenericClient* client_a = NULL;
    result = GG_GattlinkGenericClient_Create(TimerScheduler, 1024,
                                             0, 0, 100,
                                             NULL,
                                             GG_Ipv4FrameSerializer_AsFrameSerializer(frame_serializer),
                                             GG_CAST(&frame_assembler_a, GG_FrameAssembler),
                                             &client_a);
    LONGS_EQUAL(GG_SUCCESS, result);

    // create a second frame assembler
    TestFrameAssembler frame_assembler_b;
    GG_SET_INTERFACE(&frame_assembler_b, TestFrameAssembler, GG_FrameAssembler);

    // create a second gattlink client
    GG_GattlinkGenericClient* client_b = NULL;
    result = GG_GattlinkGenericClient_Create(TimerScheduler, 1024,
                                             0, 0, 100,
                                             NULL,
                                             GG_Ipv4FrameSerializer_AsFrameSerializer(frame_serializer),
                                             GG_CAST(&frame_assembler_b, GG_FrameAssembler),
                                             &client_b);
    LONGS_EQUAL(GG_SUCCESS, result);

    // create a first async pipe
    GG_AsyncPipe* client_a_transport_async_pipe = NULL;
    result = GG_AsyncPipe_Create(TimerScheduler, 4, &client_a_transport_async_pipe);
    CHECK_EQUAL(GG_SUCCESS, result);

    // create a second async pipe
    GG_AsyncPipe* client_b_transport_async_pipe = NULL;
    result = GG_AsyncPipe_Create(TimerScheduler, 4, &client_b_transport_async_pipe);
    CHECK_EQUAL(GG_SUCCESS, result);

    // setup a counting passthrough sink
    CountingPassthroughSink counting_sink;
    counting_sink.sink = GG_AsyncPipe_AsDataSink(client_b_transport_async_pipe);
    counting_sink.byte_count = 0;
    counting_sink.packet_count = 0;
    counting_sink.max_packet_size = 0;
    GG_SET_INTERFACE(&counting_sink, CountingPassthroughSink, GG_DataSink);

    result = GG_DataSource_SetDataSink(GG_GattlinkGenericClient_GetTransportSideAsDataSource(client_a),
                                       GG_CAST(&counting_sink, GG_DataSink));

    LONGS_EQUAL(GG_SUCCESS, result);

    GG_DataSource_SetDataSink(GG_AsyncPipe_AsDataSource(client_a_transport_async_pipe),
                              GG_GattlinkGenericClient_GetTransportSideAsDataSink(client_a));

    result = GG_DataSource_SetDataSink(GG_AsyncPipe_AsDataSource(client_b_transport_async_pipe),
                                       GG_GattlinkGenericClient_GetTransportSideAsDataSink(client_b));

    LONGS_EQUAL(GG_SUCCESS, result);

    result = GG_DataSource_SetDataSink(GG_GattlinkGenericClient_GetTransportSideAsDataSource(client_b),
                                       GG_AsyncPipe_AsDataSink(client_a_transport_async_pipe));

    LONGS_EQUAL(GG_SUCCESS, result);

    // start the session from both clients
    result = GG_GattlinkGenericClient_Start(client_a);
    LONGS_EQUAL(GG_SUCCESS, result);
    result = GG_GattlinkGenericClient_Start(client_b);
    LONGS_EQUAL(GG_SUCCESS, result);

    // run the timer manually for a while to let gattlink open
    advance_timer_time(100, 1);

    // create a memory sink for client b
    GG_MemoryDataSink* memory_sink;
    result = GG_MemoryDataSink_Create(&memory_sink);
    LONGS_EQUAL(GG_SUCCESS, result);

    // connect the sink to the user side of client b
    result = GG_DataSource_SetDataSink(GG_GattlinkGenericClient_GetUserSideAsDataSource(client_b),
                                       GG_MemoryDataSink_AsDataSink(memory_sink));
    LONGS_EQUAL(GG_SUCCESS, result);

    // write a buffer to the user side of client a
    uint8_t data[7] = { 1, 2, 3, 4, 5, 6, 7};
    GG_StaticBuffer send_buffer;
    GG_StaticBuffer_Init(&send_buffer, data, sizeof(data));
    result = GG_DataSink_PutData(GG_GattlinkGenericClient_GetUserSideAsDataSink(client_a),
                                 GG_StaticBuffer_AsBuffer(&send_buffer),
                                 NULL);
    LONGS_EQUAL(GG_SUCCESS, result);

    advance_timer_time(100, 1);

    LONGS_EQUAL(sizeof(data), GG_Buffer_GetDataSize(GG_MemoryDataSink_GetBuffer(memory_sink)));
    MEMCMP_EQUAL(data, GG_Buffer_GetData(GG_MemoryDataSink_GetBuffer(memory_sink)), sizeof(data));

    // max packet size should be 7+1 (1 byte header) because the largest buffer we sent was 7 bytes
    // and the MTU allows up to 100
    LONGS_EQUAL(8, counting_sink.max_packet_size);

    // reset the max packet size counter
    counting_sink.max_packet_size = 0;

    // change the MTU
    GG_GattlinkGenericClient_SetMaxTransportFragmentSize(client_a, 5);
    result = GG_DataSink_PutData(GG_GattlinkGenericClient_GetUserSideAsDataSink(client_a),
                                 GG_StaticBuffer_AsBuffer(&send_buffer),
                                 NULL);

    advance_timer_time(100, 1);

    LONGS_EQUAL(GG_SUCCESS, result);
    LONGS_EQUAL(2 * sizeof(data), GG_Buffer_GetDataSize(GG_MemoryDataSink_GetBuffer(memory_sink)));

    // write a larger buffer
    GG_DynamicBuffer* large_buffer;
    result = GG_DynamicBuffer_Create(300, &large_buffer);
    LONGS_EQUAL(GG_SUCCESS, result);
    GG_DynamicBuffer_SetDataSize(large_buffer, 300);
    memset(GG_DynamicBuffer_UseData(large_buffer), 5, 300);
    result = GG_DataSink_PutData(GG_GattlinkGenericClient_GetUserSideAsDataSink(client_a),
                                 GG_DynamicBuffer_AsBuffer(large_buffer),
                                 NULL);
    LONGS_EQUAL(GG_SUCCESS, result);

    advance_timer_time(100, 1);

    // max packet size should be 5
    LONGS_EQUAL(5, counting_sink.max_packet_size);

    // cleanup
    GG_GattlinkGenericClient_Destroy(client_a);
    GG_GattlinkGenericClient_Destroy(client_b);
    GG_MemoryDataSink_Destroy(memory_sink);
    GG_Ipv4FrameSerializer_Destroy(frame_serializer);
}

//----------------------------------------------------------------------
typedef struct {
    GG_IMPLEMENTS(GG_DataSink);

    GG_DataSink* sink;
    size_t       packet_count;
    size_t       packet_drop_interval;
} LinkConditioner;

#define PACKET_DROP_PERCENTAGE 20
#define LARGE_BUFFER_COUNT     100

//----------------------------------------------------------------------
static uint32_t trivial_rand() {
    static uint16_t lfsr = 0xACE1;
    static uint16_t bit;

    bit  = ((lfsr >> 0) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 5) ) & 1;
    lfsr = (uint16_t)((lfsr >> 1) | (bit << 15));

    return lfsr;
}

static uint32_t packet_drop_interval(uint32_t percentage) {
    return GG_MIN(10, GG_MAX(1, (100 / percentage) + (trivial_rand() % 2) - 1));
}

static GG_Result
LinkConditioner_PutData(GG_DataSink* _self, GG_Buffer* buffer, const GG_BufferMetadata* metadata)
{
    LinkConditioner* self = GG_SELF(LinkConditioner, GG_DataSink);

    // Simulate dropping a packet every so often
    if (self->packet_drop_interval > 0 &&
        self->packet_count > 0 &&
        self->packet_count % self->packet_drop_interval == 0) {
        ++self->packet_count;
        self->packet_drop_interval = packet_drop_interval(PACKET_DROP_PERCENTAGE);

        // Logging
        const uint8_t* data = (const uint8_t*)GG_Buffer_GetData(buffer);
        size_t data_len = GG_Buffer_GetDataSize(buffer);
        if ((data[0] & 0x40) == 0x40) {
            GG_LOG_INFO("Dropping Acked PSN: %d", (int)data[0] & 0x1f);
            data++;
            data_len--;
        }
        if (data_len > 0) {
            GG_LOG_INFO("Dropping PSN: %d, %d Byte(s): 0x%02hhx",
                        (int)data[0] & 0x1f,
                        (int)data_len,
                        data[1]);
        }
        return GG_SUCCESS;
    }

    GG_Result result = GG_DataSink_PutData(self->sink, buffer, metadata);
    if (GG_SUCCEEDED(result)) {
        ++self->packet_count;
    }
    return result;
}

static GG_Result
LinkConditioner_SetListener(GG_DataSink* _self, GG_DataSinkListener* listener)
{
    LinkConditioner* self = GG_SELF(LinkConditioner, GG_DataSink);
    return GG_DataSink_SetListener(self->sink, listener);
}

GG_IMPLEMENT_INTERFACE(LinkConditioner, GG_DataSink) {
    .PutData     = LinkConditioner_PutData,
    .SetListener = LinkConditioner_SetListener
};

//----------------------------------------------------------------------
TEST(GG_GENERIC_GATTLINK_CLIENT, Test_GattlinkGenericClient_DroppedPackets) {
    GG_Result result;

    // create a frame serializer
    GG_Ipv4FrameSerializer* frame_serializer;
    result = GG_Ipv4FrameSerializer_Create(NULL, &frame_serializer);
    LONGS_EQUAL(GG_SUCCESS, result);

    // create a first frame assembler
    TestFrameAssembler frame_assembler_a;
    GG_SET_INTERFACE(&frame_assembler_a, TestFrameAssembler, GG_FrameAssembler);

    // create a first gattlink client
    GG_GattlinkGenericClient* client_a = NULL;
    result = GG_GattlinkGenericClient_Create(TimerScheduler, 1024,
                                             4, 4, 10,
                                             NULL,
                                             GG_Ipv4FrameSerializer_AsFrameSerializer(frame_serializer),
                                             GG_CAST(&frame_assembler_a, GG_FrameAssembler),
                                             &client_a);
    LONGS_EQUAL(GG_SUCCESS, result);

    // create a first async pipe
    GG_AsyncPipe* client_a_transport_async_pipe = NULL;
    result = GG_AsyncPipe_Create(TimerScheduler, 4, &client_a_transport_async_pipe);
    CHECK_EQUAL(GG_SUCCESS, result);

    // connect first client transport to async pipe
    GG_DataSource_SetDataSink(GG_GattlinkGenericClient_GetTransportSideAsDataSource(client_a),
                              GG_AsyncPipe_AsDataSink(client_a_transport_async_pipe));

    // create a second frame assembler
    TestFrameAssembler frame_assembler_b;
    GG_SET_INTERFACE(&frame_assembler_b, TestFrameAssembler, GG_FrameAssembler);

    // create a second gattlink client
    GG_GattlinkGenericClient* client_b = NULL;
    result = GG_GattlinkGenericClient_Create(TimerScheduler, 1024,
                                             4, 4, 10,
                                             NULL,
                                             GG_Ipv4FrameSerializer_AsFrameSerializer(frame_serializer),
                                             GG_CAST(&frame_assembler_b, GG_FrameAssembler),
                                             &client_b);
    LONGS_EQUAL(GG_SUCCESS, result);

    // create a second first async pipe
    GG_AsyncPipe* client_b_transport_async_pipe = NULL;
    result = GG_AsyncPipe_Create(TimerScheduler, 4, &client_b_transport_async_pipe);
    CHECK_EQUAL(GG_SUCCESS, result);

    // connect second client transport to async pipe
    GG_DataSource_SetDataSink(GG_GattlinkGenericClient_GetTransportSideAsDataSource(client_b),
                              GG_AsyncPipe_AsDataSink(client_b_transport_async_pipe));

    // setup a dropping passthrough sink for both clients
    LinkConditioner client_a_link_conditioner;
    client_a_link_conditioner.sink = GG_GattlinkGenericClient_GetTransportSideAsDataSink(client_b);
    client_a_link_conditioner.packet_count = 0;
    client_a_link_conditioner.packet_drop_interval = 0;
    GG_SET_INTERFACE(&client_a_link_conditioner, LinkConditioner, GG_DataSink);

    LinkConditioner client_b_link_conditioner;
    client_b_link_conditioner.sink = GG_GattlinkGenericClient_GetTransportSideAsDataSink(client_a);
    client_b_link_conditioner.packet_count = 0;
    client_b_link_conditioner.packet_drop_interval = 0;
    GG_SET_INTERFACE(&client_b_link_conditioner, LinkConditioner, GG_DataSink);

    // connect the two gattlink clients with a dropping passthrough sink in between
    GG_DataSource_SetDataSink(GG_AsyncPipe_AsDataSource(client_a_transport_async_pipe),
                              GG_CAST(&client_a_link_conditioner, GG_DataSink));
    GG_DataSource_SetDataSink(GG_AsyncPipe_AsDataSource(client_b_transport_async_pipe),
                              GG_CAST(&client_b_link_conditioner, GG_DataSink));

    // create a memory sink for both clients
    GG_MemoryDataSink* memory_sink_a;
    result = GG_MemoryDataSink_Create(&memory_sink_a);
    LONGS_EQUAL(GG_SUCCESS, result);
    GG_MemoryDataSink* memory_sink_b;
    result = GG_MemoryDataSink_Create(&memory_sink_b);
    LONGS_EQUAL(GG_SUCCESS, result);

    // connect the sink to the user side of each client
    GG_DataSource_SetDataSink(GG_GattlinkGenericClient_GetUserSideAsDataSource(client_b),
                              GG_MemoryDataSink_AsDataSink(memory_sink_b));
    GG_DataSource_SetDataSink(GG_GattlinkGenericClient_GetUserSideAsDataSource(client_a),
                              GG_MemoryDataSink_AsDataSink(memory_sink_a));

    // start the session from both clients
    result = GG_GattlinkGenericClient_Start(client_a);
    LONGS_EQUAL(GG_SUCCESS, result);
    result = GG_GattlinkGenericClient_Start(client_b);
    LONGS_EQUAL(GG_SUCCESS, result);

    // run the timer manually for a while to let gattlink open
    advance_timer_time(100, 1);

    // start dropping packets randomly
    client_a_link_conditioner.packet_count = 0;
    client_a_link_conditioner.packet_drop_interval = packet_drop_interval(PACKET_DROP_PERCENTAGE);
    client_b_link_conditioner.packet_count = 0;
    client_b_link_conditioner.packet_drop_interval = packet_drop_interval(PACKET_DROP_PERCENTAGE);

    // write a larger buffer on both sides
    GG_DynamicBuffer* large_buffer;
    result = GG_DynamicBuffer_Create(LARGE_BUFFER_COUNT, &large_buffer);
    LONGS_EQUAL(GG_SUCCESS, result);
    GG_DynamicBuffer_SetDataSize(large_buffer, LARGE_BUFFER_COUNT);
    for (int i=0; i < LARGE_BUFFER_COUNT; i++) {
        memset(GG_DynamicBuffer_UseData(large_buffer)+i, i, 1);
    }
    result = GG_DataSink_PutData(GG_GattlinkGenericClient_GetUserSideAsDataSink(client_a),
                                 GG_DynamicBuffer_AsBuffer(large_buffer),
                                 NULL);
    LONGS_EQUAL(GG_SUCCESS, result);
    result = GG_DataSink_PutData(GG_GattlinkGenericClient_GetUserSideAsDataSink(client_b),
                                 GG_DynamicBuffer_AsBuffer(large_buffer),
                                 NULL);
    LONGS_EQUAL(GG_SUCCESS, result);

    // advance time to simulate ack transmissions and packet retransmissions
    advance_timer_time(100000, 10);

    // verify data was correctly received on both sides
    LONGS_EQUAL(LARGE_BUFFER_COUNT, GG_Buffer_GetDataSize(GG_MemoryDataSink_GetBuffer(memory_sink_a)));
    MEMCMP_EQUAL(GG_DynamicBuffer_GetData(large_buffer),
                 GG_Buffer_GetData(GG_MemoryDataSink_GetBuffer(memory_sink_a)), LARGE_BUFFER_COUNT);
    LONGS_EQUAL(LARGE_BUFFER_COUNT, GG_Buffer_GetDataSize(GG_MemoryDataSink_GetBuffer(memory_sink_b)));
    MEMCMP_EQUAL(GG_DynamicBuffer_GetData(large_buffer),
                 GG_Buffer_GetData(GG_MemoryDataSink_GetBuffer(memory_sink_b)), LARGE_BUFFER_COUNT);

    // cleanup
    GG_DataSource_SetDataSink(GG_GattlinkGenericClient_GetTransportSideAsDataSource(client_a), NULL);
    GG_DataSource_SetDataSink(GG_GattlinkGenericClient_GetTransportSideAsDataSource(client_b), NULL);
    GG_DataSource_SetDataSink(GG_AsyncPipe_AsDataSource(client_a_transport_async_pipe), NULL);
    GG_DataSource_SetDataSink(GG_AsyncPipe_AsDataSource(client_b_transport_async_pipe), NULL);
    GG_DataSource_SetDataSink(GG_GattlinkGenericClient_GetUserSideAsDataSource(client_b), NULL);
    GG_DataSource_SetDataSink(GG_GattlinkGenericClient_GetUserSideAsDataSource(client_a), NULL);
    GG_GattlinkGenericClient_Destroy(client_a);
    GG_GattlinkGenericClient_Destroy(client_b);
    GG_AsyncPipe_Destroy(client_a_transport_async_pipe);
    GG_AsyncPipe_Destroy(client_b_transport_async_pipe);
    GG_MemoryDataSink_Destroy(memory_sink_a);
    GG_MemoryDataSink_Destroy(memory_sink_b);
    GG_Ipv4FrameSerializer_Destroy(frame_serializer);
}

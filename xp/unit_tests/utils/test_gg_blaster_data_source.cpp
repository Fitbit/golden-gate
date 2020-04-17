// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

#include "CppUTest/MemoryLeakDetectorNewMacros.h"

#include "xp/module/gg_module.h"
#include "xp/common/gg_io.h"
#include "xp/common/gg_buffer.h"
#include "xp/common/gg_port.h"
#include "xp/utils/gg_blaster_data_source.h"

TEST_GROUP(GG_BLASTER)
{
    void setup(void) {
    }

    void teardown(void) {
    }
};

typedef struct {
    GG_IMPLEMENTS(GG_DataSink);

    GG_DataSinkListener*             listener;
    size_t                           counter;
    size_t                           expected_max;
    size_t                           pushback_point;
    size_t                           stop_point;
    GG_BlasterDataSourcePacketFormat packet_format;
} TestSink;

static GG_Result
TestSink_PutData(GG_DataSink* _self, GG_Buffer* data, const GG_BufferMetadata* metadata) {
    GG_COMPILER_UNUSED(metadata);
    TestSink* self = GG_SELF(TestSink, GG_DataSink);

    switch (self->packet_format) {
        case GG_BLASTER_BASIC_COUNTER_PACKET_FORMAT: {
            CHECK_TRUE(GG_Buffer_GetDataSize(data) >= 4);

            // check the counter
            uint32_t value = GG_BytesToInt32Be(GG_Buffer_GetData(data));

            if (self->expected_max == self->counter + 1) {
                CHECK_EQUAL(0xFFFFFFFF, value);
            } else {
                CHECK_EQUAL(self->counter, value);
            }
            break;
        }

        case GG_BLASTER_IP_COUNTER_PACKET_FORMAT: {
            CHECK_TRUE(GG_Buffer_GetDataSize(data) >= 20);

            const uint8_t* packet = GG_Buffer_GetData(data);

            // check the size
            uint16_t packet_size = GG_BytesToInt16Be(packet + 2);
            CHECK_EQUAL(GG_Buffer_GetDataSize(data), packet_size);

            // check the counter
            uint16_t value = GG_BytesToInt16Be(packet + 4);
            CHECK_EQUAL(self->counter, value);

            uint8_t flags = packet[6];
            if (self->expected_max == self->counter + 1) {
                CHECK_TRUE((flags & (1 << 6)) == 0);
            } else {
                CHECK_TRUE((flags & (1 << 6)) != 0);
            }
            break;
        }
    }

    // pushback if needed
    if (self->counter == self->pushback_point) {
        return GG_ERROR_WOULD_BLOCK;
    } else {
        ++self->counter;
        return GG_SUCCESS;
    }
}

static GG_Result
TestSink_SetListener(GG_DataSink* _self, GG_DataSinkListener* listener) {
    TestSink* self = GG_SELF(TestSink, GG_DataSink);
    self->listener = listener;
    return GG_SUCCESS;
}

GG_IMPLEMENT_INTERFACE(TestSink, GG_DataSink) {
    .PutData = TestSink_PutData,
    .SetListener = TestSink_SetListener
};

TEST(GG_BLASTER, Test_BlasterSource0) {
    GG_BlasterDataSource* blaster = NULL;
    GG_Result result;

    // check that passing a packet size that's too small fails
    result = GG_BlasterDataSource_Create(1,
                                         GG_BLASTER_BASIC_COUNTER_PACKET_FORMAT,
                                         0,
                                         NULL,
                                         0,
                                         &blaster);
    CHECK_EQUAL(GG_ERROR_INVALID_PARAMETERS, result);

    // check that passing a packet size that's too small fails
    result = GG_BlasterDataSource_Create(10,
                                         GG_BLASTER_IP_COUNTER_PACKET_FORMAT,
                                         0,
                                         NULL,
                                         0,
                                         &blaster);
    CHECK_EQUAL(GG_ERROR_INVALID_PARAMETERS, result);
}

TEST(GG_BLASTER, Test_BlasterSource1) {
    // setup the sink
    TestSink sink;
    sink.listener = NULL;
    sink.counter  = 0;
    sink.expected_max = 10;
    sink.pushback_point = 0;
    sink.packet_format = GG_BLASTER_BASIC_COUNTER_PACKET_FORMAT;
    GG_SET_INTERFACE(&sink, TestSink, GG_DataSink);

    // create a blaster to send 10 packet of 100 bytes
    GG_BlasterDataSource* blaster = NULL;
    GG_Result result = GG_BlasterDataSource_Create(100,
                                                   GG_BLASTER_BASIC_COUNTER_PACKET_FORMAT,
                                                   10,
                                                   NULL,
                                                   0,
                                                   &blaster);
    CHECK_EQUAL(GG_SUCCESS, result);

    // connect the sink
    GG_DataSource_SetDataSink(GG_BlasterDataSource_AsDataSource(blaster), GG_CAST(&sink, GG_DataSink));

    // start the blaster
    GG_BlasterDataSource_Start(blaster);

    // stop the blaster
    GG_BlasterDataSource_Stop(blaster);

    // reset the sink
    sink.counter  = 0;
    sink.expected_max = 10;
    sink.pushback_point = 0;

    // re-start the blaster
    GG_BlasterDataSource_Start(blaster);

    // done
    GG_BlasterDataSource_Destroy(blaster);
}

TEST(GG_BLASTER, Test_BlasterSource2) {
    // setup the sink
    TestSink sink;
    sink.listener = NULL;
    sink.counter  = 0;
    sink.expected_max = 0;
    sink.pushback_point = 10;
    sink.packet_format = GG_BLASTER_BASIC_COUNTER_PACKET_FORMAT;
    GG_SET_INTERFACE(&sink, TestSink, GG_DataSink);

    // create a blaster to send an unlimited number of packets of 100 bytes
    GG_BlasterDataSource* blaster = NULL;
    GG_Result result = GG_BlasterDataSource_Create(100,
                                                   GG_BLASTER_BASIC_COUNTER_PACKET_FORMAT,
                                                   0,
                                                   NULL,
                                                   0,
                                                   &blaster);
    CHECK_EQUAL(GG_SUCCESS, result);

    // connect the sink
    GG_DataSource_SetDataSink(GG_BlasterDataSource_AsDataSource(blaster), GG_CAST(&sink, GG_DataSink));

    // start the blaster
    GG_BlasterDataSource_Start(blaster);

    // we should have received 10 packets
    CHECK_EQUAL(10, sink.counter);

    // continue until 20
    sink.pushback_point = 20;
    GG_DataSinkListener_OnCanPut(sink.listener);
    CHECK_EQUAL(20, sink.counter);

    // done
    GG_BlasterDataSource_Destroy(blaster);
}

TEST(GG_BLASTER, Test_BlasterSource3) {
    // setup the sink
    TestSink sink;
    sink.listener = NULL;
    sink.counter  = 0;
    sink.expected_max = 0;
    sink.pushback_point = 10;
    sink.packet_format = GG_BLASTER_BASIC_COUNTER_PACKET_FORMAT;
    GG_SET_INTERFACE(&sink, TestSink, GG_DataSink);

    // crete a timer scheduler
    GG_TimerScheduler* scheduler = NULL;
    GG_Result result = GG_TimerScheduler_Create(&scheduler);
    CHECK_EQUAL(GG_SUCCESS, result);

    // create a blaster to send an unlimited number of packets of 100 bytes every 10 ms
    GG_BlasterDataSource* blaster = NULL;
    result = GG_BlasterDataSource_Create(100,
                                         GG_BLASTER_BASIC_COUNTER_PACKET_FORMAT,
                                         0,
                                         scheduler,
                                         10,
                                         &blaster);
    CHECK_EQUAL(GG_SUCCESS, result);

    // connect the sink
    GG_DataSource_SetDataSink(GG_BlasterDataSource_AsDataSource(blaster), GG_CAST(&sink, GG_DataSink));

    // start the blaster
    GG_BlasterDataSource_Start(blaster);

    // make the time advance by increments of 1 ms
    for (unsigned int i = 0; i < 100; i++) {
        GG_TimerScheduler_SetTime(scheduler, i);
    }

    CHECK_EQUAL(sink.counter, 10);

    // done
    GG_BlasterDataSource_Destroy(blaster);
    GG_TimerScheduler_Destroy(scheduler);
}

TEST(GG_BLASTER, Test_BlasterSource4) {
    // setup the sink
    TestSink sink;
    sink.listener = NULL;
    sink.counter  = 0;
    sink.expected_max = 10;
    sink.pushback_point = 0;
    sink.packet_format = GG_BLASTER_IP_COUNTER_PACKET_FORMAT;
    GG_SET_INTERFACE(&sink, TestSink, GG_DataSink);

    // create a blaster to send 10 packet of 100 bytes
    GG_BlasterDataSource* blaster = NULL;
    GG_Result result = GG_BlasterDataSource_Create(100,
                                                   GG_BLASTER_IP_COUNTER_PACKET_FORMAT,
                                                   10,
                                                   NULL,
                                                   0,
                                                   &blaster);
    CHECK_EQUAL(GG_SUCCESS, result);

    // connect the sink
    GG_DataSource_SetDataSink(GG_BlasterDataSource_AsDataSource(blaster), GG_CAST(&sink, GG_DataSink));

    // start the blaster
    GG_BlasterDataSource_Start(blaster);

    // stop the blaster
    GG_BlasterDataSource_Stop(blaster);

    // reset the sink
    sink.counter  = 0;
    sink.expected_max = 10;
    sink.pushback_point = 0;

    // re-start the blaster
    GG_BlasterDataSource_Start(blaster);

    // done
    GG_BlasterDataSource_Destroy(blaster);
}

TEST(GG_BLASTER, Test_BlasterSource5) {
    // setup the sink
    TestSink sink;
    sink.listener = NULL;
    sink.counter  = 0;
    sink.expected_max = 0;
    sink.pushback_point = 10;
    sink.packet_format = GG_BLASTER_BASIC_COUNTER_PACKET_FORMAT;
    GG_SET_INTERFACE(&sink, TestSink, GG_DataSink);

    // create a blaster to send an unlimited number of packets of 100 bytes
    GG_BlasterDataSource* blaster = NULL;
    GG_Result result = GG_BlasterDataSource_Create(100,
                                                   GG_BLASTER_BASIC_COUNTER_PACKET_FORMAT,
                                                   0,
                                                   NULL,
                                                   0,
                                                   &blaster);
    CHECK_EQUAL(GG_SUCCESS, result);

    // connect the sink
    GG_DataSource_SetDataSink(GG_BlasterDataSource_AsDataSource(blaster), GG_CAST(&sink, GG_DataSink));

    // start the blaster
    GG_BlasterDataSource_Start(blaster);

    // we should have received 10 packets
    CHECK_EQUAL(10, sink.counter);

    // stop
    GG_BlasterDataSource_Stop(blaster);

    // try to continue
    sink.pushback_point = 20;
    GG_DataSinkListener_OnCanPut(sink.listener);

    // check that we didn't continue
    CHECK_EQUAL(10, sink.counter);

    // done
    GG_BlasterDataSource_Destroy(blaster);
}

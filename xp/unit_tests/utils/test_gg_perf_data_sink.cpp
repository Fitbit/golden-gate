// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

#include "CppUTest/MemoryLeakDetectorNewMacros.h"

#include "xp/module/gg_module.h"
#include "xp/common/gg_io.h"
#include "xp/common/gg_buffer.h"
#include "xp/common/gg_port.h"
#include "xp/utils/gg_perf_data_sink.h"

TEST_GROUP(GG_PERF_SINK)
{
    void setup(void) {
    }

    void teardown(void) {
    }
};

TEST(GG_PERF_SINK, Test_PerfSink1) {
    GG_PerfDataSink* sink = NULL;

    GG_Result result = GG_PerfDataSink_Create(GG_PERF_DATA_SINK_MODE_BASIC_OR_IP_COUNTER, 0, 0, &sink);
    CHECK_EQUAL(GG_SUCCESS, result);
    CHECK_TRUE(sink != NULL);

    const GG_PerfDataSinkStats* stats = GG_PerfDataSink_GetStats(sink);
    CHECK_EQUAL(0, stats->bytes_received);
    CHECK_EQUAL(0, stats->packets_received);
    CHECK_EQUAL(0, stats->gap_count);
    CHECK_EQUAL(0, stats->last_received_counter);
    CHECK_EQUAL(0, stats->next_expected_counter);

    result = GG_DataSink_SetListener(GG_PerfDataSink_AsDataSink(sink), NULL);
    CHECK_EQUAL(GG_SUCCESS, result);

    GG_StaticBuffer packet;
    uint8_t packet_data[4] = {0, 0, 0, 0};
    GG_StaticBuffer_Init(&packet, packet_data, sizeof(packet_data));
    result = GG_DataSink_PutData(GG_PerfDataSink_AsDataSink(sink), GG_StaticBuffer_AsBuffer(&packet), NULL);
    CHECK_EQUAL(GG_SUCCESS, result);
    stats = GG_PerfDataSink_GetStats(sink);
    CHECK_EQUAL(0, stats->packets_received); // the first packet isn't counted
    CHECK_EQUAL(0, stats->bytes_received);   // the first packet isn't counted
    CHECK_EQUAL(0, stats->gap_count);
    CHECK_EQUAL(0, stats->last_received_counter);
    CHECK_EQUAL(1, stats->next_expected_counter);

    packet_data[3] = 7;
    result = GG_DataSink_PutData(GG_PerfDataSink_AsDataSink(sink), GG_StaticBuffer_AsBuffer(&packet), NULL);
    CHECK_EQUAL(GG_SUCCESS, result);
    stats = GG_PerfDataSink_GetStats(sink);
    CHECK_EQUAL(1, stats->packets_received);
    CHECK_EQUAL(4, stats->bytes_received);
    CHECK_EQUAL(1, stats->gap_count);
    CHECK_EQUAL(7, stats->last_received_counter);
    CHECK_EQUAL(8, stats->next_expected_counter);

    GG_PerfDataSink_ResetStats(sink);
    CHECK_EQUAL(0, stats->bytes_received);
    CHECK_EQUAL(0, stats->packets_received);
    CHECK_EQUAL(0, stats->gap_count);
    CHECK_EQUAL(0, stats->last_received_counter);
    CHECK_EQUAL(0, stats->next_expected_counter);

    result = GG_DataSink_PutData(GG_PerfDataSink_AsDataSink(sink), GG_StaticBuffer_AsBuffer(&packet), NULL);
    CHECK_EQUAL(GG_SUCCESS, result);
    stats = GG_PerfDataSink_GetStats(sink);
    CHECK_EQUAL(0, stats->packets_received);
    CHECK_EQUAL(0, stats->bytes_received);
    CHECK_EQUAL(1, stats->gap_count);
    CHECK_EQUAL(7, stats->last_received_counter);
    CHECK_EQUAL(8, stats->next_expected_counter);

    packet_data[3] = 8;
    result = GG_DataSink_PutData(GG_PerfDataSink_AsDataSink(sink), GG_StaticBuffer_AsBuffer(&packet), NULL);
    CHECK_EQUAL(GG_SUCCESS, result);
    stats = GG_PerfDataSink_GetStats(sink);
    CHECK_EQUAL(1, stats->packets_received);
    CHECK_EQUAL(4, stats->bytes_received);
    CHECK_EQUAL(1, stats->gap_count);
    CHECK_EQUAL(8, stats->last_received_counter);
    CHECK_EQUAL(9, stats->next_expected_counter);

    packet_data[0] = packet_data[1] = packet_data[2] = packet_data[3] = 0xFF;
    result = GG_DataSink_PutData(GG_PerfDataSink_AsDataSink(sink), GG_StaticBuffer_AsBuffer(&packet), NULL);
    CHECK_EQUAL(GG_SUCCESS, result);
    stats = GG_PerfDataSink_GetStats(sink);
    CHECK_EQUAL(2, stats->packets_received);
    CHECK_EQUAL(8, stats->bytes_received);
    CHECK_EQUAL(1, stats->gap_count);
    CHECK_EQUAL(0xFFFFFFFF, stats->last_received_counter);

    // check that the previous end of sequence marker triggers a reset on the next packet
    packet_data[0] = packet_data[1] = packet_data[2] = packet_data[3] = 0;
    result = GG_DataSink_PutData(GG_PerfDataSink_AsDataSink(sink), GG_StaticBuffer_AsBuffer(&packet), NULL);
    CHECK_EQUAL(GG_SUCCESS, result);
    stats = GG_PerfDataSink_GetStats(sink);
    CHECK_EQUAL(0, stats->bytes_received);
    CHECK_EQUAL(0, stats->packets_received);
    CHECK_EQUAL(0, stats->gap_count);
    CHECK_EQUAL(0, stats->last_received_counter);
    CHECK_EQUAL(1, stats->next_expected_counter);
}

TEST(GG_PERF_SINK, Test_PerfSinkPassthrough) {
    GG_PerfDataSink* sink1 = NULL;

    GG_Result result = GG_PerfDataSink_Create(GG_PERF_DATA_SINK_MODE_RAW, 0, 0, &sink1);
    CHECK_EQUAL(GG_SUCCESS, result);
    CHECK_TRUE(sink1 != NULL);

    GG_PerfDataSink* sink2 = NULL;

    result = GG_PerfDataSink_Create(GG_PERF_DATA_SINK_MODE_RAW, 0, 0, &sink2);
    CHECK_EQUAL(GG_SUCCESS, result);
    CHECK_TRUE(sink2 != NULL);

    // pass sink1 through to sink2
    result = GG_PerfDataSink_SetPassthroughTarget(sink1, GG_PerfDataSink_AsDataSink(sink2));
    CHECK_EQUAL(GG_SUCCESS, result);

    const GG_PerfDataSinkStats* stats1 = GG_PerfDataSink_GetStats(sink2);
    CHECK_EQUAL(0, stats1->bytes_received);
    CHECK_EQUAL(0, stats1->packets_received);
    CHECK_EQUAL(0, stats1->gap_count);
    CHECK_EQUAL(0, stats1->last_received_counter);
    CHECK_EQUAL(0, stats1->next_expected_counter);

    const GG_PerfDataSinkStats* stats2 = GG_PerfDataSink_GetStats(sink2);
    CHECK_EQUAL(0, stats2->bytes_received);
    CHECK_EQUAL(0, stats2->packets_received);
    CHECK_EQUAL(0, stats2->gap_count);
    CHECK_EQUAL(0, stats2->last_received_counter);
    CHECK_EQUAL(0, stats2->next_expected_counter);

    for (unsigned int i = 0; i < 10; i++) {
        GG_StaticBuffer packet;
        uint8_t packet_data[4] = {0, 0, 0, 0};
        GG_StaticBuffer_Init(&packet, packet_data, sizeof(packet_data));
        result = GG_DataSink_PutData(GG_PerfDataSink_AsDataSink(sink1), GG_StaticBuffer_AsBuffer(&packet), NULL);
        CHECK_EQUAL(GG_SUCCESS, result);
        stats2 = GG_PerfDataSink_GetStats(sink2);

        if (i) {
            CHECK_EQUAL(i, stats2->packets_received);
            CHECK_EQUAL(4 * i, stats2->bytes_received);
        }
    }
}

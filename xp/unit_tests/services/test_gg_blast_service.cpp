// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

#include "CppUTest/TestHarness.h"
#include "CppUTest/MemoryLeakDetectorNewMacros.h"

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <string.h>

#include "xp/module/gg_module.h"
#include "xp/common/gg_common.h"
#include "xp/services/blast/gg_blast_service.h"
#include "xp/utils/gg_perf_data_sink.h"
#include "xp/utils/gg_blaster_data_source.h"

/*----------------------------------------------------------------------
|   tests
+---------------------------------------------------------------------*/
TEST_GROUP(GG_SERVICES) {
    void setup(void) {
        GG_Module_Initialize();
    }

    void teardown(void) {
        GG_Module_Terminate();
    }
};

TEST(GG_SERVICES, Test_BlastServiceBasics) {
    GG_Loop* loop;
    GG_Result result = GG_Loop_Create(&loop);
    LONGS_EQUAL(GG_SUCCESS, result);
    result = GG_Loop_BindToCurrentThread(loop);
    LONGS_EQUAL(GG_SUCCESS, result);

    GG_BlastService* service = NULL;
    result = GG_BlastService_Create(loop, &service);
    CHECK_TRUE(result == GG_SUCCESS);
    CHECK_TRUE(service != NULL);

    GG_PerfDataSink* perf_sink = NULL;
    result = GG_PerfDataSink_Create(GG_PERF_DATA_SINK_MODE_BASIC_OR_IP_COUNTER, 0, 0, &perf_sink);
    CHECK_TRUE(result == GG_SUCCESS);

    GG_BlasterDataSource* blaster_source = NULL;
    result = GG_BlasterDataSource_Create(100,
                                         GG_BLASTER_IP_COUNTER_PACKET_FORMAT,
                                         10,
                                         GG_Loop_GetTimerScheduler(loop),
                                         0,
                                         &blaster_source);

    // attach the blast service to the local perf sink and blaster source
    GG_BlastService_Attach(service,
                           GG_BlasterDataSource_AsDataSource(blaster_source),
                           GG_PerfDataSink_AsDataSink(perf_sink));

    // start the service's blaster
    result = GG_BlastService_StartBlaster(service, 200, 5, 0);
    CHECK_TRUE(result == GG_SUCCESS);

    // start the local blaster
    result = GG_BlasterDataSource_Start(blaster_source);
    CHECK_TRUE(result == GG_SUCCESS);

    const GG_PerfDataSinkStats* perf_stats = GG_PerfDataSink_GetStats(perf_sink);
    LONGS_EQUAL(200 * (5 - 1), perf_stats->bytes_received);

    GG_PerfDataSinkStats service_stats;
    result = GG_BlastService_GetStats(service, &service_stats);
    CHECK_TRUE(result == GG_SUCCESS);
    LONGS_EQUAL(100 * (10 - 1), service_stats.bytes_received);

    GG_BlastService_ResetStats(service);
    result = GG_BlastService_GetStats(service, &service_stats);
    CHECK_TRUE(result == GG_SUCCESS);
    LONGS_EQUAL(0, service_stats.bytes_received);

    GG_Loop_Destroy(loop);
}

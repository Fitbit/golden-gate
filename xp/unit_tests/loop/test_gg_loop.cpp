// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <string.h>

#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

#include "CppUTest/MemoryLeakDetectorNewMacros.h"

#include "xp/common/gg_port.h"
#include "xp/common/gg_utils.h"
#include "xp/common/gg_memory.h"
#include "xp/loop/gg_loop.h"
#include "xp/utils/gg_blaster_data_source.h"
#include "xp/utils/gg_perf_data_sink.h"

//----------------------------------------------------------------------
TEST_GROUP(GG_LOOP)
{
    void setup(void) {
    }

    void teardown(void) {
    }
};

typedef struct {
    GG_IMPLEMENTS(GG_TimerListener);
    GG_Loop* loop;
} TestTimerListener;

static void
TestTimerListener_OnTimerFired(GG_TimerListener* _self, GG_Timer* timer, uint32_t elapsed)
{
    TestTimerListener* self = GG_SELF(TestTimerListener, GG_TimerListener);
    GG_COMPILER_UNUSED(timer);
    GG_COMPILER_UNUSED(elapsed);

    GG_Loop_RequestTermination(self->loop);
}

TEST(GG_LOOP, Test_LoopTerminationSync) {
    GG_Loop* loop = NULL;
    GG_Result result = GG_Loop_Create(&loop);
    LONGS_EQUAL(GG_SUCCESS, result);
    GG_Loop_BindToCurrentThread(loop);
    LONGS_EQUAL(GG_SUCCESS, result);
    CHECK_TRUE(loop != NULL);

    // setup the terminator
    TestTimerListener Terminator;
    GG_IMPLEMENT_INTERFACE(TestTimerListener, GG_TimerListener) {
        TestTimerListener_OnTimerFired
    };
    GG_SET_INTERFACE(&Terminator, TestTimerListener, GG_TimerListener);
    Terminator.loop = loop;

    // create a timer and schedule it for 1 second
    GG_Timer* timer;
    result = GG_TimerScheduler_CreateTimer(GG_Loop_GetTimerScheduler(loop), &timer);
    CHECK_EQUAL(GG_SUCCESS, result);
    result = GG_Timer_Schedule(timer, GG_CAST(&Terminator, GG_TimerListener), 1000);

    // run the loop until it terminates
    result = GG_Loop_Run(loop);
    CHECK_EQUAL(GG_SUCCESS, result);

    // cleanup
    GG_Timer_Destroy(timer);
    GG_Loop_Destroy(loop);
}

TEST(GG_LOOP, Test_LoopTerminationAsync) {
    GG_Loop* loop = NULL;
    GG_Result result = GG_Loop_Create(&loop);
    LONGS_EQUAL(GG_SUCCESS, result);
    GG_Loop_BindToCurrentThread(loop);
    LONGS_EQUAL(GG_SUCCESS, result);
    CHECK_TRUE(loop != NULL);

    // obtain a termination message
    GG_LoopMessage* killer = GG_Loop_CreateTerminationMessage(loop);

    // send the message to the loop with a timeout of 10 seconds
    result = GG_Loop_PostMessage(loop, killer, 10000);
    CHECK_EQUAL(GG_SUCCESS, result);

    // run the loop until it terminates
    result = GG_Loop_Run(loop);
    CHECK_EQUAL(GG_SUCCESS, result);

    GG_Loop_Destroy(loop);
}

typedef struct {
    GG_IMPLEMENTS(GG_TimerListener);
    GG_Loop* loop;
    GG_BlasterDataSource* blaster;
    unsigned int state;
} TestTimerListener2;

static void
TestTimerListener2_OnTimerFired(GG_TimerListener* _self, GG_Timer* timer, uint32_t elapsed)
{
    TestTimerListener2* self = GG_SELF(TestTimerListener2, GG_TimerListener);
    GG_COMPILER_UNUSED(timer);
    GG_COMPILER_UNUSED(elapsed);

    if (self->state == 0) {
        GG_BlasterDataSource_Stop(self->blaster);
        GG_Timer_Schedule(timer, _self, 1000); // reschedule for 1 second from now
        self->state = 1;
    } else {
        GG_Loop_RequestTermination(self->loop);
    }
}

TEST(GG_LOOP, Test_LoopSinkProxy) {
    GG_Loop* loop = NULL;
    GG_Result result = GG_Loop_Create(&loop);
    LONGS_EQUAL(GG_SUCCESS, result);
    GG_Loop_BindToCurrentThread(loop);
    LONGS_EQUAL(GG_SUCCESS, result);
    CHECK_TRUE(loop != NULL);

    GG_BlasterDataSource* blaster = NULL;
    result = GG_BlasterDataSource_Create(100,
                                         GG_BLASTER_BASIC_COUNTER_PACKET_FORMAT,
                                         1024,
                                         GG_Loop_GetTimerScheduler(loop),
                                         0,
                                         &blaster);
    LONGS_EQUAL(GG_SUCCESS, result);
    GG_PerfDataSink* perf_sink;
    result = GG_PerfDataSink_Create(GG_PERF_DATA_SINK_MODE_BASIC_OR_IP_COUNTER, 0, 1000, &perf_sink);
    LONGS_EQUAL(GG_SUCCESS, result);
    GG_LoopDataSinkProxy* proxy;
    GG_Loop_CreateDataSinkProxy(loop, 16, GG_PerfDataSink_AsDataSink(perf_sink), &proxy);
    LONGS_EQUAL(GG_SUCCESS, result);
    GG_DataSource_SetDataSink(GG_BlasterDataSource_AsDataSource(blaster), GG_LoopDataSinkProxy_AsDataSink(proxy));
    GG_BlasterDataSource_Start(blaster);

    // setup the terminator
    TestTimerListener2 state_machine;
    GG_IMPLEMENT_INTERFACE(TestTimerListener2, GG_TimerListener) {
        TestTimerListener2_OnTimerFired
    };
    GG_SET_INTERFACE(&state_machine, TestTimerListener2, GG_TimerListener);
    state_machine.loop = loop;
    state_machine.state = 0;
    state_machine.blaster = blaster;

    // create a timer and schedule it for 2 seconds
    GG_Timer* timer;
    result = GG_TimerScheduler_CreateTimer(GG_Loop_GetTimerScheduler(loop), &timer);
    CHECK_EQUAL(GG_SUCCESS, result);
    result = GG_Timer_Schedule(timer, GG_CAST(&state_machine, GG_TimerListener), 2000);

    // run the loop until it terminates
    result = GG_Loop_Run(loop);
    CHECK_EQUAL(GG_SUCCESS, result);

    const GG_PerfDataSinkStats* sink_stats = GG_PerfDataSink_GetStats(perf_sink);
    LONGS_EQUAL(0, sink_stats->gap_count);
    LONGS_EQUAL(1023, sink_stats->packets_received); // the first packet isn't counted, it just starts the timer

    // cleanup
    GG_Timer_Destroy(timer);
    GG_DataSource_SetDataSink(GG_BlasterDataSource_AsDataSource(blaster), NULL);
    GG_LoopDataSinkProxy_Destroy(proxy);
    GG_PerfDataSink_Destroy(perf_sink);
    GG_BlasterDataSource_Destroy(blaster);

    GG_Loop_Destroy(loop);
}

typedef struct {
    GG_IMPLEMENTS(GG_DataSink);
    GG_BufferMetadata* metadata;
} MetadataSink;

static GG_Result
MetadataSink_PutData(GG_DataSink* _self, GG_Buffer* data, const GG_BufferMetadata* metadata)
{
    GG_COMPILER_UNUSED(data);
    MetadataSink* self = GG_SELF(MetadataSink, GG_DataSink);
    if (metadata) {
        GG_BufferMetadata_Clone(metadata, &self->metadata);
    }
    return GG_SUCCESS;
}

static GG_Result
MetadataSink_SetListener(GG_DataSink* self, GG_DataSinkListener* listener)
{
    GG_COMPILER_UNUSED(self);
    GG_COMPILER_UNUSED(listener);
    return GG_SUCCESS;
}

GG_IMPLEMENT_INTERFACE(MetadataSink, GG_DataSink) {
    .PutData = MetadataSink_PutData,
    .SetListener = MetadataSink_SetListener
};

TEST(GG_LOOP, Test_LoopSinkProxyWithMetadata) {
    GG_Loop* loop = NULL;
    GG_Result result = GG_Loop_Create(&loop);
    LONGS_EQUAL(GG_SUCCESS, result);
    GG_Loop_BindToCurrentThread(loop);
    LONGS_EQUAL(GG_SUCCESS, result);
    CHECK_TRUE(loop != NULL);

    MetadataSink sink;
    sink.metadata = NULL;
    GG_SET_INTERFACE(&sink, MetadataSink, GG_DataSink);

    GG_LoopDataSinkProxy* proxy;
    GG_Loop_CreateDataSinkProxy(loop, 16, GG_CAST(&sink, GG_DataSink), &proxy);
    LONGS_EQUAL(GG_SUCCESS, result);

    struct {
        GG_BufferMetadata base;
        uint8_t data[10];
    } metadata;
    metadata.base.type = 1;
    metadata.base.size = sizeof(metadata);
    memset(metadata.data, 7, sizeof(metadata.data));
    GG_StaticBuffer data;
    uint8_t data_payload[4] = {1, 2, 3, 4};
    GG_StaticBuffer_Init(&data, data_payload, sizeof(data_payload));
    result = GG_DataSink_PutData(GG_CAST(&sink, GG_DataSink), GG_StaticBuffer_AsBuffer(&data), &metadata.base);
    LONGS_EQUAL(GG_SUCCESS, result);

    result = GG_Loop_PostMessage(loop, GG_Loop_CreateTerminationMessage(loop), 0);
    LONGS_EQUAL(GG_SUCCESS, result);

    result = GG_Loop_Run(loop);
    CHECK_EQUAL(GG_SUCCESS, result);
    CHECK_TRUE(sink.metadata != NULL);
    LONGS_EQUAL(metadata.base.size, sink.metadata->size);
    MEMCMP_EQUAL(&metadata, sink.metadata, metadata.base.size);
    GG_FreeMemory(sink.metadata);
    GG_Loop_Destroy(loop);
}

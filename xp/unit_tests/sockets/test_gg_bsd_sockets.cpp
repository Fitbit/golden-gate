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
#include "xp/module/gg_module.h"
#include "xp/common/gg_io.h"
#include "xp/sockets/gg_sockets.h"
#include "xp/sockets/ports/bsd/gg_bsd_sockets.h"

//----------------------------------------------------------------------
TEST_GROUP(GG_SOCKETS)
{
    void setup(void) {
    }

    void teardown(void) {
    }
};

typedef struct {
    GG_IMPLEMENTS(GG_DataSink);

    GG_Loop*           loop;
    GG_DatagramSocket* socket2;
    GG_Buffer*         last_received_data;
} SocketSink;

static GG_Result
SocketSink_PutData(GG_DataSink* _self, GG_Buffer* data, const GG_BufferMetadata* metadata)
{
    SocketSink* self = GG_SELF(SocketSink, GG_DataSink);
    GG_COMPILER_UNUSED(metadata);

    self->last_received_data = GG_Buffer_Retain(data);

    // destroy the "other" socket
    GG_DatagramSocket_Destroy(self->socket2);

    // we're done
    GG_Loop_RequestTermination(self->loop);

    return GG_SUCCESS;
}

static GG_Result
SocketSink_SetListener(GG_DataSink* self, GG_DataSinkListener* listener)
{
    GG_COMPILER_UNUSED(self);
    GG_COMPILER_UNUSED(listener);

    return GG_SUCCESS;
}

GG_IMPLEMENT_INTERFACE(SocketSink, GG_DataSink) {
    .PutData     = SocketSink_PutData,
    .SetListener = SocketSink_SetListener
};

typedef struct {
    GG_IMPLEMENTS(GG_TimerListener);

    GG_Loop* loop;
} ExitTimer;

static void
ExitTimer_OnTimerFired(GG_TimerListener* _self, GG_Timer* timer, uint32_t actual_ms_elapsed)
{
    ExitTimer* self = GG_SELF(ExitTimer, GG_TimerListener);
    GG_COMPILER_UNUSED(timer);
    GG_COMPILER_UNUSED(actual_ms_elapsed);

    GG_Loop_RequestTermination(self->loop);
}

GG_IMPLEMENT_INTERFACE(ExitTimer, GG_TimerListener) {
    .OnTimerFired = ExitTimer_OnTimerFired
};

//----------------------------------------------------------------------
TEST(GG_SOCKETS, Test_MultiSocketDestroy) {
    // create a first socket bound to a fixed port
    // (we try port numbers in sequence until we find one that's not taken)
    // (we only try ports between 2000 and 60000, that should be enough!)
    GG_DatagramSocket* socket1 = NULL;
    GG_SocketAddress local_address_1 = GG_SOCKET_ADDRESS_NULL_INITIALIZER;
    GG_Result result = GG_FAILURE;
    for (local_address_1.port = 2000; local_address_1.port <= 60000; local_address_1.port++) {
        result = GG_BsdDatagramSocket_Create(&local_address_1, NULL, false, 2000, &socket1);
        if (GG_SUCCEEDED(result)) {
            break;
        }
    }
    LONGS_EQUAL(GG_SUCCESS, result);

    // create a second socket
    GG_DatagramSocket* socket2 = NULL;
    GG_SocketAddress remote_address_2;
    remote_address_2.address = GG_IpAddress_Any;
    remote_address_2.port    = local_address_1.port;
    result = GG_BsdDatagramSocket_Create(NULL, &remote_address_2, false, 2000, &socket2);
    LONGS_EQUAL(GG_SUCCESS, result);

    // send a datagram from socket1 to socket2
    GG_DynamicBuffer* buffer;
    result = GG_DynamicBuffer_Create(5, &buffer);
    LONGS_EQUAL(GG_SUCCESS, result);
    result = GG_DynamicBuffer_SetData(buffer, (const uint8_t*)"hello", 5);
    LONGS_EQUAL(GG_SUCCESS, result);

    // create a loop and attach the sockets to it
    GG_Loop* loop = NULL;
    result = GG_Loop_Create(&loop);
    LONGS_EQUAL(GG_SUCCESS, result);
    result = GG_DatagramSocket_Attach(socket1, loop);
    LONGS_EQUAL(GG_SUCCESS, result);
    result = GG_DatagramSocket_Attach(socket2, loop);
    LONGS_EQUAL(GG_SUCCESS, result);

    // setup a sink to receive data via socket1
    SocketSink sink;
    sink.loop    = loop;
    sink.socket2 = socket2;
    sink.last_received_data = NULL;
    GG_SET_INTERFACE(&sink, SocketSink, GG_DataSink);
    GG_DataSource_SetDataSink(GG_DatagramSocket_AsDataSource(socket1),
                              GG_CAST(&sink, GG_DataSink));
    // send data through socket2
    result = GG_DataSink_PutData(GG_DatagramSocket_AsDataSink(socket2),
                                 GG_DynamicBuffer_AsBuffer(buffer),
                                 NULL);
    LONGS_EQUAL(GG_SUCCESS, result);
    GG_DynamicBuffer_Release(buffer);

    // schedule an exit timer to 2 seconds
    ExitTimer timer_handler;
    timer_handler.loop = loop;
    GG_SET_INTERFACE(&timer_handler, ExitTimer, GG_TimerListener);
    GG_Timer* timer = NULL;
    result = GG_TimerScheduler_CreateTimer(GG_Loop_GetTimerScheduler(loop), &timer);
    LONGS_EQUAL(GG_SUCCESS, result);
    GG_Timer_Schedule(timer, GG_CAST(&timer_handler, GG_TimerListener), 2000);

    // run the loop until it is interrupted
    result = GG_Loop_Run(loop);
    LONGS_EQUAL(GG_SUCCESS, result);

    CHECK_TRUE(sink.last_received_data != NULL);
    LONGS_EQUAL(5, GG_Buffer_GetDataSize(sink.last_received_data));
    MEMCMP_EQUAL("hello", GG_Buffer_GetData(sink.last_received_data), 5);
    GG_Buffer_Release(sink.last_received_data);
    GG_DatagramSocket_Destroy(socket1);

    GG_Loop_Destroy(loop);
}

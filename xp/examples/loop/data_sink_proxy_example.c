/**
 *
 * @file
 *
 * @copyright
 * Copyright 2017 by Fitbit, Inc., all rights reserved.
 *
 * @author Gilles Boccon-Gibod
 *
 * @date 2017-10-15
 *
 * @details
 *
 * Example of how to use loop data sink proxies
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <inttypes.h>

#include "xp/common/gg_port.h"
#include "xp/common/gg_results.h"
#include "xp/common/gg_io.h"
#include "xp/common/gg_utils.h"
#include "xp/common/gg_threads.h"
#include "xp/loop/gg_loop.h"

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
typedef struct {
    GG_IMPLEMENTS(GG_DataSinkListener);

    GG_Mutex* mutex;
    uint32_t  counter;
} PushSource;

typedef struct {
    GG_IMPLEMENTS(GG_DataSink);
    GG_IMPLEMENTS(GG_TimerListener);

    GG_DataSinkListener* listener;
    GG_Timer*            timer;
    bool                 busy;
    bool                 have_waiter;
} PacedSink;

/*----------------------------------------------------------------------
|   globals
+---------------------------------------------------------------------*/
static PacedSink Sink;
static GG_LoopDataSinkProxy* SinkProxy = NULL;
static PushSource Source;

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/
static GG_Result
PacedSink_PutData(GG_DataSink* _self, GG_Buffer* data, const GG_BufferMetadata* metadata)
{
    PacedSink* self = GG_SELF(PacedSink, GG_DataSink);
    GG_COMPILER_UNUSED(metadata);

    if (self->busy) {
        printf("=== paced sink busy, must retry later\n");
        self->have_waiter = true;
        return GG_ERROR_WOULD_BLOCK;
    }

    uint32_t counter = GG_BytesToInt32Be(GG_Buffer_GetData(data));
    printf("@@@ got data: %u in thread %" PRIx64 "\n", (int)counter, (uint64_t)GG_GetCurrentThreadId());
    self->busy = true;

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
static GG_Result
PacedSink_SetListener(GG_DataSink* _self, GG_DataSinkListener* listener)
{
    PacedSink* self = GG_SELF(PacedSink, GG_DataSink);

    self->listener = listener;

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
static void
PacedSink_OnTimerFired(GG_TimerListener* _self, GG_Timer* timer, uint32_t elapsed)
{
    PacedSink* self = GG_SELF(PacedSink, GG_TimerListener);
    GG_COMPILER_UNUSED(elapsed);

    printf("--- timer fired\n");
    GG_Timer_Schedule(timer, _self, 5000);
    self->busy = false;
    if (self->have_waiter) {
        self->have_waiter = false;
        GG_DataSinkListener_OnCanPut(self->listener);
    }
}

//----------------------------------------------------------------------
static void
PushSource_TryToPush(PushSource* self)
{
    // protect this function because it may be called from different threads
    GG_Mutex_Lock(self->mutex);

    // create a buffer with the next message
    GG_DynamicBuffer* buffer;
    GG_DynamicBuffer_Create(4, &buffer);
    uint8_t bytes[4];
    GG_BytesFromInt32Be(bytes, self->counter);
    GG_DynamicBuffer_SetData(buffer, bytes, 4);

    // send the data to the sink proxy
    GG_Result result = GG_DataSink_PutData(GG_LoopDataSinkProxy_AsDataSink(SinkProxy),
                                           GG_DynamicBuffer_AsBuffer(buffer),
                                           NULL);
    printf("### sending %u to proxy from thread %" PRIx64 ", result=%d\n",
           self->counter,
           (uint64_t)GG_GetCurrentThreadId(),
           result);

    if (GG_SUCCEEDED(result)) {
        ++self->counter;
    }

    GG_DynamicBuffer_Release(buffer);

    GG_Mutex_Unlock(self->mutex);
}

//----------------------------------------------------------------------
static void
PushSource_OnCanPut(GG_DataSinkListener* _self)
{
    PushSource* self = GG_SELF(PushSource, GG_DataSinkListener);

    PushSource_TryToPush(self);
}

GG_IMPLEMENT_INTERFACE(PacedSink, GG_DataSink) {
    PacedSink_PutData,
    PacedSink_SetListener
};

GG_IMPLEMENT_INTERFACE(PacedSink, GG_TimerListener) {
    PacedSink_OnTimerFired
};

GG_IMPLEMENT_INTERFACE(PushSource, GG_DataSinkListener) {
    PushSource_OnCanPut
};

//----------------------------------------------------------------------
static void*
thread_run(void* arg)
{
    GG_COMPILER_UNUSED(arg);

    printf("+++ thread ID = %" PRIx64 "\n", (uint64_t)GG_GetCurrentThreadId());

    for (;;) {
        printf("*** waiting 1 second\n");
        usleep(1000000);

        PushSource_TryToPush(&Source);
    }

    return NULL;
}

//----------------------------------------------------------------------
int
main(int argc, char** argv)
{
    GG_COMPILER_UNUSED(argc);
    GG_COMPILER_UNUSED(argv);

    printf("=== Golden Gate Data Sink Proxy Loop Example ===\n");

    // setup a sink
    Sink.listener = NULL;
    Sink.busy     = false;
    GG_SET_INTERFACE(&Sink, PacedSink, GG_DataSink);
    GG_SET_INTERFACE(&Sink, PacedSink, GG_TimerListener);

    // setup a PushSource source
    Source.counter = 0;
    GG_Mutex_Create(&Source.mutex);
    GG_SET_INTERFACE(&Source, PushSource, GG_DataSinkListener);

    // setup a loop
    GG_Loop* loop = NULL;
    GG_Loop_Create(&loop);
    GG_Loop_BindToCurrentThread(loop);

    // setup a sink proxy
    GG_Loop_CreateDataSinkProxy(loop, 3, GG_CAST(&Sink, GG_DataSink), &SinkProxy);

    // register the source as a listener for the sink proxy
    GG_DataSink_SetListener(GG_LoopDataSinkProxy_AsDataSink(SinkProxy),
                            GG_CAST(&Source, GG_DataSinkListener));

    // create a timer for the sink and set it for an initial interval
    GG_TimerScheduler_CreateTimer(GG_Loop_GetTimerScheduler(loop), &Sink.timer);
    GG_Timer_Schedule(Sink.timer, GG_CAST(&Sink, GG_TimerListener), 5000);

    printf("=== spawning thread\n");

    pthread_t thread;
    pthread_create(&thread, NULL, thread_run, loop);

    printf("+++ running loop\n");
    GG_Loop_Run(loop);
    printf("--- loop done\n");

    GG_Loop_Destroy(loop);
    return 0;
}

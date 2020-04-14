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
 * Example of how to use loop data sink listener proxies
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
#include "xp/common/gg_system.h"
#include "xp/loop/gg_loop.h"

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
typedef struct {
    GG_IMPLEMENTS(GG_DataSink);

    GG_Mutex*            mutex;
    GG_DataSinkListener* listener;
    bool                 have_waiter;
    unsigned int         will_accept;
} PacedReceiver;

typedef struct {
    GG_IMPLEMENTS(GG_DataSinkListener);

    GG_DataSink* sink;
    uint32_t     counter;
    GG_Buffer*   pending_data;
} PushSource;

/*----------------------------------------------------------------------
|   globals
+---------------------------------------------------------------------*/
static PushSource Source;
static GG_LoopDataSinkListenerProxy* SinkListenerProxy = NULL;
static PacedReceiver Sink;

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/
static void
PushSource_TryToSend(PushSource* self)
{
    GG_Result result;
    if (self->pending_data) {
        // try to re-send
        result = GG_DataSink_PutData(self->sink, self->pending_data, NULL);
        if (GG_FAILED(result)) return;

        // done with this buffer
        GG_Buffer_Release(self->pending_data);
        self->pending_data = NULL;
    }

    do {
        // create a buffer with the next message
        GG_DynamicBuffer* buffer;
        GG_DynamicBuffer_Create(4, &buffer);
        uint8_t bytes[4];
        GG_BytesFromInt32Be(bytes, self->counter);
        GG_DynamicBuffer_SetData(buffer, bytes, 4);
        ++self->counter;

        // try to send the message
        result = GG_DataSink_PutData(self->sink, GG_DynamicBuffer_AsBuffer(buffer), NULL);

        if (GG_FAILED(result)) {
            // couldn't send, keep it pending
            self->pending_data = GG_DynamicBuffer_AsBuffer(buffer);
        } else {
            GG_DynamicBuffer_Release(buffer);
        }
    } while (GG_SUCCEEDED(result));
}

//----------------------------------------------------------------------
static void
PushSource_OnCanPut(GG_DataSinkListener* _self)
{
    PushSource* self = GG_SELF(PushSource, GG_DataSinkListener);

    printf("=== OnCanPut called on thread %" PRIx64 "\n", (uint64_t)GG_GetCurrentThreadId());

    PushSource_TryToSend(self);
}

//----------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(PushSource, GG_DataSinkListener) {
    PushSource_OnCanPut
};

//----------------------------------------------------------------------
static GG_Result
PacedReceiver_PutData(GG_DataSink* _self, GG_Buffer* data, const GG_BufferMetadata* metadata)
{
    PacedReceiver* self = GG_SELF(PacedReceiver, GG_DataSink);
    GG_Result result = GG_SUCCESS;
    GG_COMPILER_UNUSED(metadata);

    GG_Mutex_Lock(self->mutex);

    if (self->will_accept == 0) {
        self->have_waiter = true;
        result = GG_ERROR_WOULD_BLOCK;
        printf("--- sink busy, will need to try again later\n");
    } else {
        uint32_t counter = GG_BytesToInt32Be(GG_Buffer_GetData(data));
        printf("@@@ got data %u, size=%u, will_accept=%u, on thread %" PRIx64 "\n",
               counter,
               (unsigned int)GG_Buffer_GetDataSize(data),
               self->will_accept,
               (uint64_t)GG_GetCurrentThreadId());
        --self->will_accept;
    }

    GG_Mutex_Unlock(self->mutex);

    return result;
}

//----------------------------------------------------------------------
static GG_Result
PacedReceiver_SetListener(GG_DataSink* _self, GG_DataSinkListener* listener)
{
    PacedReceiver* self = GG_SELF(PacedReceiver, GG_DataSink);

    self->listener = listener;

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(PacedReceiver, GG_DataSink) {
    PacedReceiver_PutData,
    PacedReceiver_SetListener
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

        GG_Mutex_Lock(Sink.mutex);

        // pick an increment between 0 and 3 based on the current timestamp
        unsigned int accept_increment = (GG_System_GetCurrentTimestamp() >> 8) % 3;
        Sink.will_accept += accept_increment;
        printf("*** increasing accept count by %d on thread %" PRIx64 "\n",
               accept_increment,
               (uint64_t)GG_GetCurrentThreadId());

        // notify the listener if needed
        if (Sink.have_waiter) {
            GG_DataSinkListener_OnCanPut(Sink.listener);
            Sink.have_waiter = false;
        }

        GG_Mutex_Unlock(Sink.mutex);
    }

    return NULL;
}

//----------------------------------------------------------------------
int
main(int argc, char** argv)
{
    GG_COMPILER_UNUSED(argc);
    GG_COMPILER_UNUSED(argv);

    printf("=== Golden Gate Data Sink Listener Proxy Loop Example ===\n");

    // setup a loop
    GG_Loop* loop = NULL;
    GG_Loop_Create(&loop);
    GG_Loop_BindToCurrentThread(loop);

    // setup a sink
    GG_Mutex_Create(&Sink.mutex);
    Sink.listener    = NULL;
    Sink.will_accept = 0;
    GG_SET_INTERFACE(&Sink, PacedReceiver, GG_DataSink);

    // setup a source
    Source.counter = 0;
    Source.pending_data = NULL;
    Source.sink = GG_CAST(&Sink, GG_DataSink);
    GG_SET_INTERFACE(&Source, PushSource, GG_DataSinkListener);

    // setup a sink listener proxy
    GG_Loop_CreateDataSinkListenerProxy(loop, GG_CAST(&Source, GG_DataSinkListener), &SinkListenerProxy);

    // register the listener proxy with the sink
    GG_DataSink_SetListener(GG_CAST(&Sink, GG_DataSink),
                            GG_LoopDataSinkListenerProxy_AsDataSinkListener(SinkListenerProxy));

    // prime the pump
    PushSource_TryToSend(&Source);

    printf("=== spawning thread\n");

    pthread_t thread;
    pthread_create(&thread, NULL, thread_run, loop);

    printf("+++ running loop\n");
    GG_Loop_Run(loop);
    printf("--- loop done\n");

    GG_Loop_Destroy(loop);
    return 0;
}

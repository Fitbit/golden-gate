// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <pthread.h>
#include <unistd.h>

#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

#include "CppUTest/MemoryLeakDetectorNewMacros.h"

#include "xp/common/gg_port.h"
#include "xp/common/gg_utils.h"
#include "xp/common/gg_timer.h"
#include "xp/common/gg_system.h"
#include "xp/loop/gg_loop.h"
#include "xp/common/gg_threads.h"

//----------------------------------------------------------------------
TEST_GROUP(GG_LOOP_WITH_THREADS)
{
    void setup(void) {
    }

    void teardown(void) {
    }
};

static void*
thread_run(void* arg)
{
    GG_Loop* loop = (GG_Loop*)arg;

    GG_ThreadGuard_SetMainLoopThreadId(GG_GetCurrentThreadId());

    // sleep a bit to not dequeue messages right away
    usleep(100 * GG_MICROSECONDS_PER_MILLISECOND);

    GG_Loop_Run(loop);

    return NULL;
}

typedef struct {
    int a;
    int b;
} TestArgs;

static int
test_sync_function(void* arg) {
    TestArgs* my_args = (TestArgs*)arg;

    return 1234 + (my_args->a * my_args->b);
}

static int test_async_value = 1234;
static void
test_async_function(void* arg) {
    TestArgs* my_args = (TestArgs*)arg;

    test_async_value += (my_args->a * my_args->b);
}

typedef struct {
    GG_IMPLEMENTS(GG_TimerListener);
    GG_Loop* loop;
} TestTimerListener;

static GG_Timestamp start_time;
static GG_Timestamp end_time;

static void
TestTimerListener_OnTimerFired(GG_TimerListener* _self, GG_Timer* timer, uint32_t elapsed) {
    (void)timer;
    (void)elapsed;
    TestTimerListener* self = GG_SELF(TestTimerListener, GG_TimerListener);
    GG_LoopMessage* bye_bye_message = GG_Loop_CreateTerminationMessage(self->loop);
    CHECK_TRUE(bye_bye_message != NULL);
    GG_Result result = GG_Loop_PostMessage(self->loop, bye_bye_message, GG_TIMEOUT_INFINITE);
    LONGS_EQUAL(GG_SUCCESS, result);

    end_time = GG_System_GetCurrentTimestamp();
}

GG_IMPLEMENT_INTERFACE(TestTimerListener, GG_TimerListener) {
    .OnTimerFired = TestTimerListener_OnTimerFired
};

static GG_Timer* timer;
static TestTimerListener timer_listener;
static int
test_sync_function2(void* arg) {
    CHECK_TRUE(GG_ThreadGuard_CheckCurrentThreadIsMainLoop(NULL));

    GG_Loop* loop = (GG_Loop*)arg;
    GG_TimerScheduler* scheduler = GG_Loop_GetTimerScheduler(loop);
    GG_TimerScheduler_CreateTimer(scheduler, &timer);
    GG_SET_INTERFACE(&timer_listener, TestTimerListener, GG_TimerListener);
    timer_listener.loop = loop;
    GG_Timer_Schedule(timer, GG_CAST(&timer_listener, GG_TimerListener), 1000); // timer for 1 second from now

    start_time = GG_System_GetCurrentTimestamp();

    return 0;
}

TEST(GG_LOOP_WITH_THREADS, Test_LoopMessage) {
    GG_Loop* loop = NULL;
    GG_Result result = GG_Loop_Create(&loop);
    LONGS_EQUAL(GG_SUCCESS, result);

    pthread_t thread;
    pthread_create(&thread, NULL, thread_run, loop);

    // queue more than the default loop queue size to reach the point where it starts blocking
    int test_async_check = 1234;
    TestArgs async_args[100];
    for (int i = 0; i < 100; i++) {
        async_args[i].a = 7;
        async_args[i].b = i;
        result = GG_Loop_InvokeAsync(loop, test_async_function, &async_args[i]);
        CHECK_EQUAL(GG_SUCCESS, result);
        test_async_check += async_args[i].a * async_args[i].b;
    }

    for (int i = 0; i < 100; i++) {
        TestArgs sync_args = {
            7,
            i
        };
        int f_result = 0;
        result = GG_Loop_InvokeSync(loop, test_sync_function, &sync_args, &f_result);
        CHECK_EQUAL(GG_SUCCESS, result);
        LONGS_EQUAL(1234 + (sync_args.a * sync_args.b), f_result);
    }

    // at this point, all the async messages are known to have been handled
    LONGS_EQUAL(test_async_check, test_async_value);

    // invoke a function that will setup a timer
    GG_Loop_InvokeSync(loop, test_sync_function2, loop, NULL);

    // wait for the loop to be done
    int presult = pthread_join(thread, NULL);
    CHECK_EQUAL(0, presult);

    // check that the time measured by the test timer is close to 1 second
    GG_TimeInterval elapsed = end_time - start_time;
    GG_TimeInterval diff = elapsed > GG_NANOSECONDS_PER_SECOND ?
                           (elapsed - GG_NANOSECONDS_PER_SECOND) :
                           (GG_NANOSECONDS_PER_SECOND - elapsed);
    CHECK_TRUE(diff < GG_NANOSECONDS_PER_SECOND / 10);

    CHECK_FALSE(GG_ThreadGuard_CheckCurrentThreadIsMainLoop(NULL));

    GG_Loop_Destroy(loop);
}

typedef struct {
    GG_Loop*  loop;
    int       x;
} InvokeMeArgs;

static int
invoke_me(void* args)
{
    CHECK_TRUE(args != NULL);
    InvokeMeArgs* me = (InvokeMeArgs*)args;
    if (me->loop) {
        InvokeMeArgs next_args = {
            .loop = NULL,
            .x = me->x * 2
        };
        int invoke_result = 0;
        GG_Result result = GG_Loop_InvokeSync(me->loop, invoke_me, &next_args, &invoke_result);
        LONGS_EQUAL(GG_SUCCESS, result);

        return invoke_result;
    }

    return me->x;
}

TEST(GG_LOOP_WITH_THREADS, Test_LoopInvokeSync) {
    GG_Loop* loop = NULL;
    GG_Result result = GG_Loop_Create(&loop);
    LONGS_EQUAL(GG_SUCCESS, result);
    result = GG_Loop_BindToCurrentThread(loop);
    LONGS_EQUAL(GG_SUCCESS, result);

    // try first without recursion
    int invoke_result = 0;
    InvokeMeArgs args = {
        .loop = NULL,
        .x = 7
    };
    result = GG_Loop_InvokeSync(loop, invoke_me, &args, &invoke_result);
    LONGS_EQUAL(GG_SUCCESS, result);
    LONGS_EQUAL(7, invoke_result);

    // now try with recursion
    invoke_result = 0;
    args = (InvokeMeArgs) {
        .loop = loop,
        .x = 9
    };
    result = GG_Loop_InvokeSync(loop, invoke_me, &args, &invoke_result);
    LONGS_EQUAL(GG_SUCCESS, result);
    LONGS_EQUAL(18, invoke_result);

    GG_Loop_Destroy(loop);
}

TEST(GG_LOOP_WITH_THREADS, Test_LoopBinding) {
    GG_Loop* loop;
    GG_Result result = GG_Loop_Create(&loop);
    LONGS_EQUAL(GG_SUCCESS, result);

    // check that we can bind the loop
    result = GG_Loop_BindToCurrentThread(loop);
    LONGS_EQUAL(GG_SUCCESS, result);

    // check that trying to bind an already-bound loop fails
    result = GG_Loop_BindToCurrentThread(loop);
    LONGS_EQUAL(GG_ERROR_INVALID_STATE, result);

    GG_Loop_Destroy(loop);
}

static int
my_func(void* arg) {
    GG_Loop* loop = (GG_Loop*)arg;

    GG_Loop_RequestTermination(loop);

    return 123;
}

static void*
thread_run2(void* arg)
{
    GG_Loop* loop = (GG_Loop*)arg;

    // sleep a bit so that the invoking thread gets to run first
    usleep(100 * GG_MICROSECONDS_PER_MILLISECOND);

    GG_Loop_Run(loop);

    return NULL;
}

TEST(GG_LOOP_WITH_THREADS, Test_LoopBinding2) {
    // create a loop but do not bind it
    GG_Loop* loop;
    GG_Result result = GG_Loop_Create(&loop);
    LONGS_EQUAL(GG_SUCCESS, result);

    // start a thread to run the loop;
    pthread_t thread;
    pthread_create(&thread, NULL, thread_run2, loop);

    // invoke sync a function (this should wait until the thread starts and runs the loop
    int function_result;
    result = GG_Loop_InvokeSync(loop, my_func, loop, &function_result);
    LONGS_EQUAL(GG_SUCCESS, result);
    LONGS_EQUAL(123, function_result);

    // wait for the thread to be done
    pthread_join(thread, NULL);

    // cleanup
    GG_Loop_Destroy(loop);
}

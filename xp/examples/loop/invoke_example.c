/**
 *
 * @file
 *
 * @copyright
 * Copyright 2017-2020 Fitbit, Inc
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author Gilles Boccon-Gibod
 *
 * @date 2019-09-18
 *
 * @details
 *
 * loop invoke example
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

#include "xp/common/gg_port.h"
#include "xp/common/gg_results.h"
#include "xp/common/gg_threads.h"
#include "xp/loop/gg_loop.h"
#include "xp/module/gg_module.h"

//----------------------------------------------------------------------
static int
func1(void* arg)
{
    unsigned int* i = (unsigned int*)arg;
    printf("--- func1 invoked in thread %x, arg=%u\n", (int)GG_GetCurrentThreadId(), *i);
    return (int)(*i * 2);
}

//----------------------------------------------------------------------
typedef struct {
    unsigned int sequence;
} Func2Message;

static void
func2(void* arg)
{
    Func2Message* message = (Func2Message*)arg;
    printf("--- func2 invoked in thread %x, sequence=%u\n", (int)GG_GetCurrentThreadId(), message->sequence);
    GG_FreeMemory(message);
}

//----------------------------------------------------------------------
static void*
thread_run(void* arg)
{
    GG_Loop* loop = (GG_Loop*)arg;

    for (unsigned int i = 0; i < 10; i++) {
        printf("### waiting 1 second\n");
        usleep(1000000);

        if (i % 2 == 0) {
            printf("*** [%u] invoking func1 synchronously from thread %x\n", i, (int)GG_GetCurrentThreadId());
            int result = 0;
            GG_Loop_InvokeSync(loop, func1, &i, &result);
            printf("*** [%u] func1 result = %d\n", i, result);
        } else {
            printf("*** [%u] invoking func2 asynchronously from thread %x\n", i, (int)GG_GetCurrentThreadId());
            Func2Message* message = GG_AllocateZeroMemory(sizeof(Func2Message));
            message->sequence = i;
            GG_Loop_InvokeAsync(loop, func2, message);
        }
    }

    GG_LoopMessage* bye_bye_message = GG_Loop_CreateTerminationMessage(loop);
    GG_Loop_PostMessage(loop, bye_bye_message, GG_TIMEOUT_INFINITE);

    return NULL;
}

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/
int
main(int argc, char** argv)
{
    GG_COMPILER_UNUSED(argc);
    GG_COMPILER_UNUSED(argv);

    printf("=== Golden Gate Loop Invoke Example ===\n");

    // init Golden Gate
    GG_Module_Initialize();

    // setup a loop
    GG_Loop* loop;
    GG_Loop_Create(&loop);
    GG_Loop_BindToCurrentThread(loop);

    // spawn a thread
    pthread_t thread;
    pthread_create(&thread, NULL, thread_run, loop);

    // run the loop
    printf("@@@ running the loop in thread %x\n", (int)GG_GetCurrentThreadId());
    GG_Loop_Run(loop);
    printf("@@@ the loop has terminated");

    // cleanup
    GG_Loop_Destroy(loop);

    // wait for the thread to finish
    pthread_join(thread, NULL);

    return 0;
}

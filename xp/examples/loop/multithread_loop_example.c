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
 * @date 2017-09-26
 *
 * @details
 *
 * Simple multithreaded loop example
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

#include "xp/common/gg_port.h"
#include "xp/common/gg_results.h"
#include "xp/loop/gg_loop.h"

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
typedef struct {
    GG_IMPLEMENTS(GG_LoopMessage);

    unsigned int counter;
} Task;

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/
static void
Task_Handle(GG_LoopMessage* _self)
{
    Task* self = GG_SELF(Task, GG_LoopMessage);

    printf("### Task tick %d\n", self->counter);
    ++self->counter;
}

static void
Task_Release(GG_LoopMessage* _self)
{
    GG_COMPILER_UNUSED(_self);
}

GG_IMPLEMENT_INTERFACE(Task, GG_LoopMessage) {
    Task_Handle,
    Task_Release
};

static void*
thread_run(void* arg)
{
    GG_Loop* loop = (GG_Loop*)arg;

    Task task;
    task.counter = 0;
    GG_SET_INTERFACE(&task, Task, GG_LoopMessage);

    for (;;) {
        printf("*** waiting 1 second\n");
        usleep(1000000);

        printf("*** dispatching to loop\n");
        GG_Loop_PostMessage(loop, GG_CAST(&task, GG_LoopMessage), GG_TIMEOUT_INFINITE);
    }

    return NULL;
}

int
main(int argc, char** argv)
{
    GG_COMPILER_UNUSED(argc);
    GG_COMPILER_UNUSED(argv);

    printf("=== Golden Gate Multithreaded Loop Example ===\n");

    // create and run a loop
    GG_Loop* loop = NULL;
    GG_Result result = GG_Loop_Create(&loop);
    GG_ASSERT(GG_SUCCEEDED(result));
    GG_COMPILER_UNUSED(result);
    GG_Loop_BindToCurrentThread(loop);
    GG_ASSERT(GG_SUCCEEDED(result));

    printf("=== spawning thread\n");

    pthread_t thread;
    pthread_create(&thread, NULL, thread_run, loop);

    printf("+++ running loop\n");
    GG_Loop_Run(loop);
    printf("--- loop done\n");

    pthread_join(thread, NULL);
    GG_Loop_Destroy(loop);
    return 0;
}

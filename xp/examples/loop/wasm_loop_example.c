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
 * @date 2019-02-21
 *
 * @details
 *
 * WASM loop example
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdio.h>

#include "xp/module/gg_module.h"
#include "xp/common/gg_common.h"
#include "xp/loop/gg_loop.h"

/*----------------------------------------------------------------------
|   globals
+---------------------------------------------------------------------*/
static GG_Loop* g_loop;
static GG_Timer* g_timer;


/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

//----------------------------------------------------------------------
static void
on_timer_fired(GG_TimerListener* self, GG_Timer* timer, uint32_t elapsed)
{
    GG_COMPILER_UNUSED(self);
    GG_COMPILER_UNUSED(timer);
    GG_COMPILER_UNUSED(elapsed);
    printf("### Timer fired!\n");
}

static GG_TimerListener* g_timer_listener = &GG_INTERFACE_INLINE_UNNAMED(GG_TimerListener, {
    .OnTimerFired = on_timer_fired
});

//----------------------------------------------------------------------
int
main(int argc, char** argv)
{
    GG_COMPILER_UNUSED(argc);
    GG_COMPILER_UNUSED(argv);

    printf("=== Golden Gate Timer Loop Example ===\n");

    // init Golden Gate
    GG_Module_Initialize();

    // setup a loop
    GG_Loop_Create(&g_loop);
    GG_Loop_BindToCurrentThread(g_loop);

    // schedule a timer
    GG_TimerScheduler_CreateTimer(GG_Loop_GetTimerScheduler(g_loop), &g_timer);
    GG_Timer_Schedule(g_timer, g_timer_listener, 3000);
    return 0;
}

//----------------------------------------------------------------------
void
terminate(void)
{
    GG_Loop_Destroy(g_loop);
    g_loop = NULL;
}

//----------------------------------------------------------------------
int
do_work(void)
{
    printf("do_work\n");
    if (!g_loop) {
        return -1;
    }
    uint32_t next_timer = 0;
    GG_Result result = GG_Loop_DoWork(g_loop, 0, &next_timer);
    if (GG_FAILED(result)) {
        return result;
    }
    if (next_timer > 0xFFFF) {
        next_timer = 0xFFFF;
    }
    return (int)next_timer;
}

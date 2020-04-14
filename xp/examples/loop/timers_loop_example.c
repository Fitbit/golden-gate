/**
 *
 * @file
 *
 * @copyright
 * Copyright 2017 by Fitbit, Inc., all rights reserved.
 *
 * @author Gilles Boccon-Gibod
 *
 * @date 2017-10-04
 *
 * @details
 *
 * Simple multithreaded loop example
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdio.h>

#include "xp/common/gg_port.h"
#include "xp/common/gg_results.h"
#include "xp/loop/gg_loop.h"
#include "xp/module/gg_module.h"

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/
static GG_TimerListener timer1_listener;
static void
OnTimer1Fired(GG_TimerListener *self, GG_Timer *timer, uint32_t time_elapsed)
{
    GG_COMPILER_UNUSED(self);
    GG_COMPILER_UNUSED(timer);
    printf("=== timer 1 fired, elapsed=%u, rescheduling for 1000ms\n", time_elapsed);
    GG_Timer_Schedule(timer, (GG_TimerListener*)&timer1_listener, 1000);
}

static GG_TimerListener timer2_listener;
static void
OnTimer2Fired(GG_TimerListener *self, GG_Timer *timer, uint32_t time_elapsed)
{
    GG_COMPILER_UNUSED(self);
    GG_COMPILER_UNUSED(timer);
    printf("### timer 2 fired, elapsed=%u, rescheduling for 3003ms\n", time_elapsed);
    GG_Timer_Schedule(timer, (GG_TimerListener*)&timer2_listener, 3003);
}

static GG_TimerListener timer3_listener;
static void
OnTimer3Fired(GG_TimerListener *self, GG_Timer *timer, uint32_t time_elapsed)
{
    GG_COMPILER_UNUSED(self);
    GG_COMPILER_UNUSED(timer);
    static uint32_t seed = 123456789;
    seed = (1103515245 * seed + 12345);
    printf("@@@ timer 3 fired, elapsed=%u, rescheduling for %dms\n", time_elapsed, seed % 10000);
    GG_Timer_Schedule(timer, (GG_TimerListener*)&timer3_listener, seed % 10000);
}

GG_IMPLEMENT_INTERFACE(Timer1, GG_TimerListener) {
    OnTimer1Fired
};
GG_IMPLEMENT_INTERFACE(Timer2, GG_TimerListener) {
    OnTimer2Fired
};
GG_IMPLEMENT_INTERFACE(Timer3, GG_TimerListener) {
    OnTimer3Fired
};

int
main(int argc, char** argv)
{
    GG_COMPILER_UNUSED(argc);
    GG_COMPILER_UNUSED(argv);

    printf("=== Golden Gate Timer Loop Example ===\n");

    // init Golden Gate
    GG_Module_Initialize();

    // setup a loop
    GG_Loop* loop;
    GG_Loop_Create(&loop);
    GG_Loop_BindToCurrentThread(loop);

    // get the loop's timer scheduler
    GG_TimerScheduler* scheduler = GG_Loop_GetTimerScheduler(loop);

    // create timers
    GG_Timer* timer1;
    GG_Timer* timer2;
    GG_Timer* timer3;
    GG_TimerScheduler_CreateTimer(scheduler, &timer1);
    GG_TimerScheduler_CreateTimer(scheduler, &timer2);
    GG_TimerScheduler_CreateTimer(scheduler, &timer3);

    // setup the listeners
    GG_INTERFACE(&timer1_listener) = &Timer1_GG_TimerListenerInterface;
    GG_INTERFACE(&timer2_listener) = &Timer2_GG_TimerListenerInterface;
    GG_INTERFACE(&timer3_listener) = &Timer3_GG_TimerListenerInterface;

    // schedule the timers
    GG_Timer_Schedule(timer1, (GG_TimerListener*)&timer1_listener, 1000);
    GG_Timer_Schedule(timer2, (GG_TimerListener*)&timer2_listener, 3003);
    GG_Timer_Schedule(timer3, (GG_TimerListener*)&timer3_listener, 2000);

    // run the loop
    printf("+++ running loop\n");
    GG_Loop_Run(loop);
    printf("--- loop done\n");

    GG_Loop_Destroy(loop);
    return 0;
}

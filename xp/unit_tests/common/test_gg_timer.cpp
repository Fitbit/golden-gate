// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

#include "CppUTest/MemoryLeakDetectorNewMacros.h"

extern "C" {
  #include "xp/common/gg_port.h"
  #include "xp/common/gg_timer.h"
}

static GG_TimerListener timer1_listener;

static void
OnTimer1Fired(GG_TimerListener *self, GG_Timer *timer, uint32_t time_elapsed)
{
    GG_COMPILER_UNUSED(self);
    GG_COMPILER_UNUSED(time_elapsed);
    GG_Timer_Unschedule(timer);
    GG_Timer_Destroy(timer);
    timer = NULL;
}

GG_IMPLEMENT_INTERFACE(Timer1, GG_TimerListener) {
    OnTimer1Fired
};


TEST_GROUP(GG_TIMER)
{
  void setup(void) {
  }

  void teardown(void) {
  }
};

// A dead simple example test. We should add more coverage

TEST(GG_TIMER, Test_TimerSchedulerCreateAndDestroy) {
  GG_TimerScheduler* scheduler = NULL;
  GG_Result result = GG_TimerScheduler_Create(&scheduler);
  CHECK(GG_SUCCEEDED(result));
  GG_TimerScheduler_Destroy(scheduler);
}

TEST(GG_TIMER, Test_TimerGetRemainingTime) {
  GG_Timer* timer = NULL;
  GG_TimerScheduler* scheduler = NULL;
  GG_Result result = GG_TimerScheduler_Create(&scheduler);
  CHECK(GG_SUCCEEDED(result));

  result = GG_TimerScheduler_CreateTimer(scheduler, &timer);
  CHECK(GG_SUCCEEDED(result));

  // set current time to 0ms
  result = GG_TimerScheduler_SetTime(scheduler, 0);
  CHECK(GG_SUCCEEDED(result));

  uint32_t time_delta = GG_Timer_GetRemainingTime(timer);
  CHECK(time_delta == 0);

  // schedule timer to fire at 1000ms
  GG_INTERFACE(&timer1_listener) = &Timer1_GG_TimerListenerInterface;
  GG_Timer_Schedule(timer, (GG_TimerListener*)&timer1_listener, 1000);

  time_delta = GG_Timer_GetRemainingTime(timer);
  CHECK(time_delta == 1000);

  // set current time to 2000ms
  result = GG_TimerScheduler_SetTime(scheduler, 2000);
  CHECK(GG_SUCCEEDED(result));

  // check if remaining time is 0 once timer has fired
  time_delta = GG_Timer_GetRemainingTime(timer);
  CHECK(time_delta == 0);

  GG_Timer_Destroy(timer);
  GG_TimerScheduler_Destroy(scheduler);
}

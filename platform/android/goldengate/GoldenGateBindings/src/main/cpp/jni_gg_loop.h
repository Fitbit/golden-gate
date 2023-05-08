// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

#include <jni.h>
#include "third_party/goldengate/xp/loop/gg_loop.h"

extern "C" {

GG_Loop* Loop_GetLoop();

GG_TimerScheduler* Loop_GetTimerScheduler();

JNIEnv* Loop_GetJNIEnv();

GG_Result Loop_InvokeSync(GG_LoopSyncFunction function,
                          void* function_argument,
                          int* function_result);

GG_Result Loop_InvokeAsync(GG_LoopAsyncFunction function,
                           void* function_argument);

}

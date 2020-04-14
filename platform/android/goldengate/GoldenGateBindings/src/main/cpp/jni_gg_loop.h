#include <xp/loop/gg_loop.h>
#include "jni.h"

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

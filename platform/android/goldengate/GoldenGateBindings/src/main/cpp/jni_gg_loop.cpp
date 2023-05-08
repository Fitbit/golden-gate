// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

#include <cassert>
#include <pthread.h>
#include <string.h>
#include "third_party/goldengate/platform/android/goldengate/GoldenGateBindings/src/main/cpp/jni_gg_loop.h"
#include "third_party/goldengate/platform/android/goldengate/GoldenGateBindings/src/main/cpp/logging/jni_gg_logging.h"
#include "third_party/goldengate/xp/common/gg_memory.h"

extern "C" {

static volatile struct {
    GG_Loop *loop;
    JNIEnv *env;
} looper;

static pthread_mutex_t looper_lock = PTHREAD_MUTEX_INITIALIZER;

JNIEXPORT void
JNICALL
Java_com_fitbit_goldengate_bindings_RunLoop_startLoopJNI(
        JNIEnv *env,
        jobject thiz/* this */,
        jclass clazz) {
    GG_Log_JNI("RunLoop", "Starting loop from JNI");
    jmethodID callback = env->GetMethodID(clazz, "onLoopCreated", "()V");
    looper.env = env;

    // ensure the loop exists
    GG_Loop* loop = Loop_GetLoop();

    // notify that the loop is about to start
    env->CallVoidMethod(thiz, callback);

    // run the loop
    GG_Loop_Run(loop);
}

JNIEXPORT void
JNICALL
Java_com_fitbit_goldengate_bindings_RunLoop_destroyLoopJNI(
        JNIEnv *env,
        jobject /* this */) {
        GG_Loop_Destroy(looper.loop);
}

JNIEXPORT void
JNICALL
Java_com_fitbit_goldengate_bindings_RunLoop_stopLoopJNI(
        JNIEnv *env,
        jobject /* this */) {
    GG_Loop_RequestTermination(looper.loop);
}

GG_Loop* Loop_GetLoop() {
    int result = pthread_mutex_lock(&looper_lock);
    assert(result == 0);

    // auto-create the loop if it doesn't exist yet
    if (!looper.loop) {
        GG_Loop* loop = NULL;
        GG_Loop_Create(&loop);
        looper.loop = loop;
    }

    result = pthread_mutex_unlock(&looper_lock);
    assert(result == 0);

    return looper.loop;
}

GG_TimerScheduler* Loop_GetTimerScheduler() {
    return GG_Loop_GetTimerScheduler(Loop_GetLoop());
}

JNIEnv* Loop_GetJNIEnv() {
    assert(looper.env != NULL);
    return looper.env;
}

/* Use for calling synchronous callback methods on the GG_Loop thread */
GG_Result Loop_InvokeSync(GG_LoopSyncFunction function,
                          void* function_argument,
                          int* function_result) {
    return GG_Loop_InvokeSync(Loop_GetLoop(), function, function_argument, function_result);
}

/* Use for making asynchronous calls on the GG_Loop thread */
GG_Result Loop_InvokeAsync(GG_LoopAsyncFunction function,
                           void* function_argument) {
    return GG_Loop_InvokeAsync(Loop_GetLoop(), function, function_argument);
}
}

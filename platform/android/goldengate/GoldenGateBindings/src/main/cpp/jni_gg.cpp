// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

#include "third_party/goldengate/platform/android/goldengate/GoldenGateBindings/src/main/cpp/jni_gg.h"
#include "third_party/goldengate/platform/android/goldengate/GoldenGateBindings/src/main/cpp/jni_gg_loop.h"
#include "third_party/goldengate/xp/module/gg_module.h"
#include "third_party/goldengate/xp/common/gg_common.h"

extern "C"
JNIEXPORT jint
JNICALL
Java_com_fitbit_goldengate_bindings_GoldenGate_initModulesJNI(
        JNIEnv *env,
        jobject thiz/* this */) {
    return GG_Module_Initialize();
}

extern "C"
JNIEXPORT jobject
JNICALL
Java_com_fitbit_goldengate_bindings_GoldenGate_getVersionJNI(
        JNIEnv *env,
        jobject /* this */,
        jclass clazz) {
    uint16_t    maj;
    uint16_t    min;
    uint16_t    patch;
    uint32_t    commit_count;
    const char* commit_hash;
    const char* branch_name;
    const char* build_date;
    const char* build_time;
    GG_Version(&maj, &min, &patch, &commit_count, &commit_hash, &branch_name, &build_date, &build_time);
    jmethodID constructor = env->GetMethodID(
            clazz, "<init>",
            "(JJJILjava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");
    return env->NewObject(clazz, constructor, maj, min, patch, commit_count,
                          env->NewStringUTF(commit_hash),
                          env->NewStringUTF(branch_name),
                          env->NewStringUTF(build_date),
                          env->NewStringUTF(build_time));
}

static int pingGG(void* args) {
    static int counter = 0;
    return counter++;
}

extern "C"
JNIEXPORT jint
JNICALL
Java_com_fitbit_goldengate_bindings_GoldenGate_pingJNI(
        JNIEnv *env,
        jobject /* this */) {
    int counter;
    Loop_InvokeSync(pingGG, NULL, &counter);
    return counter;
}

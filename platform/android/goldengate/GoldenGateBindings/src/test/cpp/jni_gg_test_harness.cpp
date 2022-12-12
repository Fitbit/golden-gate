// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

#include <cstring>
#include <gtest/gtest.h>
#include "platform/android/goldengate/GoldenGateBindings/src/test/cpp/jni_gg_test_harness.h"


JNIEnv *jniEnv;

extern "C"
JNIEXPORT jint
JNICALL
Java_com_fitbit_goldengate_bindings_JNITestHarness_runTestsJNI(JNIEnv *env,
                                                  jobject /* this */,
                                                  jstring methodArgs) {
    jniEnv = env;
    int argc = 2;
    const char *argv[] = {"java", "--coverage"};

    const char* args =  env->GetStringUTFChars(methodArgs, NULL);
    testing::GTEST_FLAG(filter) = args;
    testing::InitGoogleTest(&argc, (char**) argv);

    fflush(stdout);

    int result = RUN_ALL_TESTS();

    fflush(stdout);

    env->ReleaseStringUTFChars(methodArgs, args);
    jniEnv = NULL;

    return result;
}

JNIEnv* getJNIEnv() {
    return jniEnv;
}

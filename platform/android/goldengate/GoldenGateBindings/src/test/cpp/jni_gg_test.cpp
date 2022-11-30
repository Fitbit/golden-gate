// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

#include <jni.h>
#include <gtest/gtest.h>
#include "platform/android/goldengate/GoldenGateBindings/src/main/cpp/jni_gg.h"
#include "platform/android/goldengate/GoldenGateBindings/src/test/cpp/jni_gg_test_harness.h"
#include "xp/module/gg_module.h"
#include "xp/common/gg_common.h"

TEST(GG_Module, Initialize) {
    GG_Result result = Java_com_fitbit_goldengate_bindings_GoldenGate_initModulesJNI(getJNIEnv(), NULL);
    EXPECT_EQ(GG_SUCCESS, result);
}

TEST(GG_Module, GetVersion) {
    JNIEnv *env = getJNIEnv();
    jclass clazz = env->FindClass("com/fitbit/goldengate/bindings/GoldenGate$Version");
    jobject jobj = Java_com_fitbit_goldengate_bindings_GoldenGate_getVersionJNI(env, NULL, clazz);
    ASSERT_NE(nullptr, jobj);
}

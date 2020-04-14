#include <jni.h>
#include <xp/module/gg_module.h>
#include <xp/common/gg_common.h>
#include <gtest/gtest.h>
#include <jni_gg.h>
#include "jni_gg_test_harness.h"

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

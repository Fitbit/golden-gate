// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <xp/utils/gg_blaster_data_source.h>
#include <util/jni_gg_utils.h>
#include <jni_gg_test_harness.h>

TEST(Blaster, Setup) {
    GG_BlasterDataSource* blaster;
    GG_Result result = setupBlaster(&blaster, GG_BLASTER_IP_COUNTER_PACKET_FORMAT);
    ASSERT_EQ(false, blaster == NULL);
    ASSERT_EQ(0, result);
}

TEST(PerfDataSink, Setup) {
    GG_PerfDataSink* sink;
    setupPerfSink(&sink);
    ASSERT_EQ(false, sink == NULL);
}

TEST(PerfDataSink, GetStats) {
    JNIEnv *env = getJNIEnv();
    GG_PerfDataSink* sink;
    setupPerfSink(&sink);
    jclass clazz = env->FindClass("com/fitbit/goldengate/bindings/util/PerfDataStats");
    jobject  jobj = getPerfDataSinkStats(env, sink, clazz);
    ASSERT_EQ(false, jobj == NULL);
    ASSERT_EQ(true, checkObjectIsOfTypeClass(env, jobj, "com.fitbit.goldengate.bindings.util.PerfDataStats"));
}

static jbyteArray createTestByteArray(JNIEnv* env) {
    jbyteArray result;
    result = env->NewByteArray(8);
    jbyte resultType[8] = {5, 3, 7, 8, 6, 84, 82, 6};
    env->SetByteArrayRegion(result, 0, 8, resultType);
    return result;
}

TEST(Buffer, ToByteArray) {
    JNIEnv *env = getJNIEnv();
    uint8_t arr[8] = {5, 3, 7, 8, 6, 84, 82, 6};
    GG_DynamicBuffer* buffer;
    GG_DynamicBuffer_Create(8, &buffer);
    GG_DynamicBuffer_SetData(buffer, arr, 8);
    jbyteArray jarr = ggBufferToJByteArray(env, GG_DynamicBuffer_AsBuffer(buffer));
    ASSERT_EQ(false, jarr == NULL);
    ASSERT_EQ(8, env->GetArrayLength(jarr));
    jbyte *bytes = env->GetByteArrayElements(jarr, NULL);
    for (int i = 0; i < 8; i++) {
        ASSERT_EQ(arr[i], (uint8_t) bytes[i]);
    }
    env->ReleaseByteArrayElements(jarr, bytes, JNI_ABORT);
    env->DeleteLocalRef(jarr);
}

TEST(ByteArray, ToBuffer) {
    JNIEnv *env = getJNIEnv();
    jbyteArray array = createTestByteArray(env);
    GG_Buffer *buffer;
    jbyteArrayToGG_Buffer(env, array, &buffer);
    ASSERT_EQ(false, buffer == NULL);
}

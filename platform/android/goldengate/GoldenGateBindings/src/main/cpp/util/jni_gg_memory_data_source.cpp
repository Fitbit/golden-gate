// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

#include "third_party/goldengate/platform/android/goldengate/GoldenGateBindings/src/main/cpp/jni_gg_loop.h"
#include "third_party/goldengate/platform/android/goldengate/GoldenGateBindings/src/main/cpp/logging/jni_gg_logging.h"
#include "third_party/goldengate/platform/android/goldengate/GoldenGateBindings/src/main/cpp/util/jni_gg_utils.h"
#include "third_party/goldengate/xp/common/gg_common.h"
#include "third_party/goldengate/xp/utils/gg_memory_data_source.h"

/**
 * Jni Bindings for MemoryDataSource
 */
extern "C" {

    JNIEXPORT jlong
    JNICALL
    Java_com_fitbit_goldengate_bindings_util_MemoryDataSource_create(
            JNIEnv * env,
            jobject thiz,
            jbyteArray dataByteArray,
            jint chunkSize
    ) {
        GG_Buffer* data_buffer;
        GG_Result result = jbyteArrayToGG_Buffer(env, dataByteArray, &data_buffer);
        if (GG_FAILED(result)) {
            GG_Log_JNI("MemoryDataSource", "jbyteArrayToGG_Buffer failed with error code %d", result);
        }
        GG_MemoryDataSource* memory_source;
        result = GG_MemoryDataSource_Create(data_buffer, (size_t) chunkSize, &memory_source);
        if (GG_FAILED(result)) {
            GG_Log_JNI("MemoryDataSource", "GG_MemoryDataSource_Create failed with error code %d", result);
        }

        return (jlong) (intptr_t) memory_source;
    }

    JNIEXPORT void
    JNICALL
    Java_com_fitbit_goldengate_bindings_util_MemoryDataSource_destroy(
            JNIEnv * env,
            jobject thiz,
            jlong selfPtr
    ) {
        GG_MemoryDataSource* self = (GG_MemoryDataSource*) (intptr_t) selfPtr;
        GG_MemoryDataSource_Destroy(self);
    }

    JNIEXPORT void
    JNICALL
    Java_com_fitbit_goldengate_bindings_util_MemoryDataSource_attach(
            JNIEnv * env,
            jobject thiz,
            jlong selfPtr,
            jlong dataSinkPtr
    ) {
        GG_MemoryDataSource* self = (GG_MemoryDataSource*) (intptr_t) selfPtr;
        GG_ASSERT(self);
        GG_DataSink* data_sink = (GG_DataSink *) (intptr_t) dataSinkPtr;

        GG_Result result = GG_DataSource_SetDataSink(
                GG_MemoryDataSource_AsDataSource(self),
                data_sink);
        if (GG_FAILED(result)) {
            GG_Log_JNI("MemoryDataSource", "GG_DataSource_SetDataSink failed with error code %d", result);
        }
    }

    JNIEXPORT void
    JNICALL
    Java_com_fitbit_goldengate_bindings_util_MemoryDataSource_start(
            JNIEnv * env,
            jobject thiz,
            jlong selfPtr
    ) {
        GG_MemoryDataSource* self = (GG_MemoryDataSource*) (intptr_t) selfPtr;
        GG_ASSERT(self);

        GG_Result result = GG_MemoryDataSource_Start(self);
        if (GG_FAILED(result)) {
            GG_Log_JNI("MemoryDataSource", "GG_MemoryDataSource_Start failed with error code %d", result);
        }
    }
}

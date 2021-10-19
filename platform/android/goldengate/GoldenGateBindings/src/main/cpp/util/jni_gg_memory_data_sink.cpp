// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

#include "platform/android/goldengate/GoldenGateBindings/src/main/cpp/jni_gg_loop.h"
#include "platform/android/goldengate/GoldenGateBindings/src/main/cpp/logging/jni_gg_logging.h"
#include "platform/android/goldengate/GoldenGateBindings/src/main/cpp/util/jni_gg_utils.h"
#include "xp/common/gg_common.h"
#include "xp/utils/gg_memory_data_sink.h"

/**
 * Jni Bindings for MemoryDataSink
 */
extern "C" {

    JNIEXPORT jlong
    JNICALL
    Java_com_fitbit_goldengate_bindings_util_MemoryDataSink_create(
            JNIEnv * env,
            jobject thiz
    ) {
        GG_MemoryDataSink* memory_sink;
        GG_Result result = GG_MemoryDataSink_Create(&memory_sink);
        if (GG_FAILED(result)) {
            GG_Log_JNI("MemoryDataSink", "GG_MemoryDataSink_Create failed with error code %d", result);
        }

        return (jlong) (intptr_t) memory_sink;
    }

    JNIEXPORT void
    JNICALL
    Java_com_fitbit_goldengate_bindings_util_MemoryDataSink_destroy(
            JNIEnv * env,
            jobject thiz,
            jlong selfPtr
    ) {
        GG_MemoryDataSink* self = (GG_MemoryDataSink*) (intptr_t) selfPtr;
        GG_MemoryDataSink_Destroy(self);
    }

    JNIEXPORT void
    JNICALL
    Java_com_fitbit_goldengate_bindings_util_MemoryDataSink_attach(
            JNIEnv * env,
            jobject thiz,
            jlong selfPtr,
            jlong dataSourcePtr
    ) {
        GG_MemoryDataSink* self = (GG_MemoryDataSink*) (intptr_t) selfPtr;
        GG_ASSERT(self);
        GG_DataSource* data_source = (GG_DataSource *) (intptr_t) dataSourcePtr;
        GG_ASSERT(data_source);

        GG_Result result = GG_DataSource_SetDataSink(
                data_source,
                GG_MemoryDataSink_AsDataSink(self));
        if (GG_FAILED(result)) {
            GG_Log_JNI("MemoryDataSink", "GG_DataSource_SetDataSink failed with error code %d", result);
        }
    }

    JNIEXPORT jbyteArray
    JNICALL
    Java_com_fitbit_goldengate_bindings_util_MemoryDataSink_getBuffer(
            JNIEnv * env,
            jobject thiz,
            jlong selfPtr
    ) {
        GG_MemoryDataSink* self = (GG_MemoryDataSink*) (intptr_t) selfPtr;
        GG_ASSERT(self);

        GG_Buffer* buffer = GG_MemoryDataSink_GetBuffer(self);
        return ggBufferToJByteArray(env, buffer);
    }
}

// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

#include <cstring>
#include "third_party/goldengate/platform/android/goldengate/GoldenGateBindings/src/main/cpp/logging/jni_gg_logging.h"
#include "third_party/goldengate/platform/android/goldengate/GoldenGateBindings/src/main/cpp/jni_gg_loop.h"
#include "third_party/goldengate/platform/android/goldengate/GoldenGateBindings/src/main/cpp/util/jni_gg_utils.h"
#include "third_party/goldengate/xp/loop/gg_loop.h"


#define BLASTER_PACKET_SIZE          512

GG_Result setupBlaster(GG_BlasterDataSource **blasterDataSource, GG_BlasterDataSourcePacketFormat format) {
    GG_Result result = GG_BlasterDataSource_Create(BLASTER_PACKET_SIZE,
                                                   format,
                                                   0,
                                                   Loop_GetTimerScheduler(),
                                                   0,
                                                   blasterDataSource);
    if (result < 0) {
        GG_Log_JNI("JNI GG Utils", "Could not create blaster data source");
    } else {
        GG_Log_JNI("JNI GG Utils", "Successfully created blaster data source");
    }
    return result;
}

GG_Result setupPerfSink(GG_PerfDataSink** perfDataSink) {
    GG_Result result = GG_PerfDataSink_Create(GG_PERF_DATA_SINK_MODE_RAW,
                           GG_PERF_DATA_SINK_OPTION_PRINT_STATS_TO_LOG,
                           1000, perfDataSink);
    if (result < 0) {
        GG_Log_JNI("JNI GG Utils", "Could not create perf data sink");
    } else {
        GG_Log_JNI("JNI GG Utils", "Successfully created perf data sink");
    }
    return result;
}

jobject getPerfDataSinkStats(JNIEnv *env, GG_PerfDataSink *sink, jclass clazz) {
    if (sink == NULL) {
        GG_Log_JNI("JNI GG Utils", "Not a valid sink pointer");
        return NULL;
    }
    const GG_PerfDataSinkStats *stats = GG_PerfDataSink_GetStats(sink);
    if (stats != NULL) {
        jmethodID constructor = env->GetMethodID(clazz, "<init>", "(JJJJJJJ)V");
        return env->NewObject(clazz, constructor, stats->packets_received, stats->bytes_received,
                              stats->throughput, stats->last_received_counter,
                              stats->next_expected_counter, stats->gap_count,
                              stats->passthrough_would_block_count);
    } else {
        return NULL;
    }
}

GG_Result jbyteArrayToGG_Buffer(JNIEnv* env, jbyteArray array, GG_Buffer** outBuffer) {
    GG_DynamicBuffer* buffer;
    uint8_t* arrayBytes = (uint8_t *) env->GetByteArrayElements(array, NULL);
    size_t arraySize = (size_t) env->GetArrayLength(array);

    GG_Result result = GG_DynamicBuffer_Create(arraySize, &buffer);
    if (GG_SUCCEEDED(result)) {
        result = GG_DynamicBuffer_SetData(buffer, arrayBytes, arraySize);
        if (GG_SUCCEEDED(result)) {
            *outBuffer = GG_DynamicBuffer_AsBuffer(buffer);
        }
    }

    env->ReleaseByteArrayElements(array, (jbyte*) arrayBytes, JNI_ABORT);

    return result;
}

jbyteArray ggBufferToJByteArray(JNIEnv* env, GG_Buffer* data) {
    size_t dataSize = GG_Buffer_GetDataSize(data);
    jbyteArray dataByteArray = env->NewByteArray(dataSize);
    env->SetByteArrayRegion(dataByteArray, 0, dataSize, (jbyte*) GG_Buffer_GetData(data));
    return dataByteArray;
}

/*
 * className should be the fully qualified name e.g. "java.lang.String"
 */

bool checkObjectIsOfTypeClass(JNIEnv *env, jobject object, const char* className) {
    jclass clazz = env->GetObjectClass(object);
    jmethodID getclass    = env->GetMethodID(clazz, "getClass", "()Ljava/lang/Class;");
    jobject   clazzObj   = env->CallObjectMethod(object, getclass);
    jclass    cls  = env->GetObjectClass(clazzObj);
    jmethodID getName = env->GetMethodID(cls, "getName", "()Ljava/lang/String;");
    jstring name = static_cast<jstring>(env->CallObjectMethod(clazzObj, getName));
    const char* clazzName = env->GetStringUTFChars(name, NULL);
    int result =  strncmp(className, clazzName, strlen(className));
    env->DeleteLocalRef(clazzObj);
    env->ReleaseStringUTFChars(name, clazzName);
    return result == 0;
}

void printStackTrace(JNIEnv* env, jthrowable throwable) {
    jclass clazz = env->GetObjectClass(throwable);
    jmethodID printStackTrace = env->GetMethodID(clazz, "printStackTrace", "()V");
    env->CallVoidMethod(throwable, printStackTrace);
    env->DeleteLocalRef(clazz);
}

jbyteArray JByteArray_From_DataPointer(JNIEnv *env, jbyte *data, jsize data_size) {
    jbyteArray _data = env->NewByteArray(data_size);
    env->SetByteArrayRegion(_data, 0, data_size, data);
    return _data;
}

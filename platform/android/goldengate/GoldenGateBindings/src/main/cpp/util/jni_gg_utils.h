#include <jni.h>
#include <xp/utils/gg_blaster_data_source.h>
#include <xp/utils/gg_perf_data_sink.h>

#ifndef GOLDENGATE_JNI_GG_UTILS_H
#define GOLDENGATE_JNI_GG_UTILS_H

GG_Result setupBlaster(GG_BlasterDataSource **blasterDataSource, GG_BlasterDataSourcePacketFormat format);

GG_Result setupPerfSink(GG_PerfDataSink** perfDataSink);

jobject getPerfDataSinkStats(JNIEnv *env, GG_PerfDataSink *sink, jclass clazz);

/**
 * This function takes in a jbyteArray and copies its contents to a GG_Buffer
 * Since GG_Buffers are refcounted, no special handling of the outBuffer is required.
 * @param env A JNIEnv that can be called from the current thread.
 * @param array The jbyteArray that should be copied into outBuffer
 * @param outBuffer A pointer to receive the GG_Buffer created by this method.
 * @return
 */
GG_Result jbyteArrayToGG_Buffer(JNIEnv* env, jbyteArray array, GG_Buffer** outBuffer);

/**
 * This function takes in a GG_Buffer and copies its contents into a jbyteArray
 * Be sure to call env->DeleteLocalRef on the value returned from this function when you're done with it.
 * @param env A JNIEnv that can be called from the current thread.
 * @param data The GG_Buffer that should be copied into a jbyteArray
 * @return a jbyteArray with the same contents as the data argument. Call env->DeleteLocalRef on it when done.
 */
jbyteArray ggBufferToJByteArray(JNIEnv* env, GG_Buffer* data);

bool checkObjectIsOfTypeClass(JNIEnv *env, jobject jobj, const char* className);

void printStackTrace(JNIEnv* env, jthrowable throwable);

/**
 * Create a [jbyteArray] from given native byte array pointer.
 *
 * Note: Caller must delete reference to returned [jbyteArray] after its usage
 */
jbyteArray JByteArray_From_DataPointer(JNIEnv *env, jbyte *data, jsize data_size);

#endif // GOLDENGATE_JNI_GG_UTILS_H

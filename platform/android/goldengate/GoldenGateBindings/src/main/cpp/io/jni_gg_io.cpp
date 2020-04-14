#include <string.h>
#include <xp/loop/gg_loop.h>
#include <xp/common/gg_port.h>
#include <xp/common/gg_memory.h>
#include "jni_gg_io.h"
#include <jni_gg_loop.h>
#include <logging/jni_gg_logging.h>
#include <util/jni_gg_utils.h>

extern "C" {

//----------------------------------------------------------------------
GG_Result
TxSink_PutData(GG_DataSink *_self, GG_Buffer *data, const GG_BufferMetadata *metadata) {
    GG_COMPILER_UNUSED(metadata);
    TxSink *self = GG_SELF(TxSink, GG_DataSink);
    if (self->receiver) {
        const jbyte *jbyteData = (jbyte *) GG_Buffer_GetData(data);
        jsize dataSize = (jsize) GG_Buffer_GetDataSize(data);
        JNIEnv *env = Loop_GetJNIEnv();
        jbyteArray dataArray = env->NewByteArray(dataSize);
        GG_Log_JNI("TxSink", "Calling into Java PutData callback");
        env->SetByteArrayRegion(dataArray, 0, dataSize, jbyteData);
        env->CallVoidMethod(self->receiver, self->callback, dataArray);
        env->DeleteLocalRef(dataArray);
        return GG_SUCCESS;
    }
    GG_Log_JNI("TxSink", "TxSink has been destroyed. Returning WOULD_BLOCK");
    return GG_ERROR_WOULD_BLOCK;
}

//----------------------------------------------------------------------
static GG_Result
TxSink_SetListener(GG_DataSink *_self, GG_DataSinkListener *listener) {
    GG_COMPILER_UNUSED(_self);
    GG_COMPILER_UNUSED(listener);
    // do nothing, as we don't support calling back listeners in this sink implementation
    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(TxSink, GG_DataSink) {
    TxSink_PutData,
    TxSink_SetListener
};

JNIEXPORT jlong
JNICALL
Java_com_fitbit_goldengate_bindings_io_TxSink_create(
        JNIEnv *env,
        jobject thiz/* this */,
        jclass clazz,
        jstring methodName,
        jstring methodSignature) {
    // create a TxSink
    TxSink *sink = (TxSink *) GG_AllocateZeroMemory(sizeof(TxSink));
    GG_SET_INTERFACE(sink, TxSink, GG_DataSink);
    // set up receiver and method ID
    const char *method = env->GetStringUTFChars(methodName, NULL);
    const char *signature = env->GetStringUTFChars(methodSignature, NULL);
    jmethodID callback = env->GetMethodID(clazz, method, signature);
    env->ReleaseStringUTFChars(methodName, method);
    env->ReleaseStringUTFChars(methodSignature, signature);
    GG_Log_JNI("TxSink", "Creating TxSink");
    sink->callback = callback;
    jobject thizz = env->NewGlobalRef(
            thiz); // This is required to have a jobject we can share across JNI calls.
    sink->receiver = thizz;
    return (jlong) (intptr_t) sink;
}

JNIEXPORT void
JNICALL
Java_com_fitbit_goldengate_bindings_io_TxSink_destroy(
        JNIEnv *env,
        jobject thiz, /* this */
        jlong txSinkPointer) {
    TxSink *sink = (TxSink *) (intptr_t) txSinkPointer;
    env->DeleteGlobalRef(sink->receiver);
    GG_Log_JNI("TxSink", "Freeing tx sink");
    GG_ClearAndFreeObject(sink, 1);
}

static GG_Result RxSource_SetDataSink(GG_DataSource* _self, GG_DataSink *dataSink) {
    RxSource* self = GG_SELF(RxSource, GG_DataSource);

    //  de-register as a listener from the current sink
    if (self->sink) {
        // release the current proxy
        GG_DataSink_SetListener(self->sink, nullptr);
    }

    // keep a reference to the new sink
    self->sink = dataSink;

    if (dataSink) {
        GG_Log_JNI("RxSource", "Setting new tx sink");
        // register with the sink as a listener to know when we can try to send
        GG_DataSink_SetListener(dataSink, GG_CAST(self, GG_DataSinkListener));
    }

    return GG_SUCCESS;
}

GG_IMPLEMENT_INTERFACE(RxSource, GG_DataSource) {
    RxSource_SetDataSink
};

JNIEXPORT jlong
JNICALL
Java_com_fitbit_goldengate_bindings_io_RxSource_create(
        JNIEnv *env,
        jobject thiz/* this */) {
    RxSource *transportSource = (RxSource *) GG_AllocateZeroMemory(sizeof(RxSource));
    GG_Log_JNI("RxSource", "Creating RxSource");
    GG_SET_INTERFACE(transportSource, RxSource, GG_DataSource);
    return (jlong) (intptr_t) transportSource;
}

JNIEXPORT void
JNICALL
Java_com_fitbit_goldengate_bindings_io_RxSource_destroy(
        JNIEnv *env,
        jobject thiz, /* this */
        jlong rxSourcePointer) {
    RxSource *source = (RxSource *) (intptr_t) rxSourcePointer;
    GG_Log_JNI("RxSource", "Destroying RxSource");
    RxSource_SetDataSink(GG_CAST(source, GG_DataSource), nullptr);
    GG_ClearAndFreeObject(source, 1);
}

typedef struct {
    RxSource* rxSource;
    jsize dataSize;
    jbyte* data;
} RxSourceDataArgs;

GG_Result handleReceiveData(void* args) {
    RxSourceDataArgs *rxDataArgs = (RxSourceDataArgs *) args;

    if (rxDataArgs->rxSource->sink) {
        GG_DynamicBuffer *message_buffer;
        GG_DynamicBuffer_Create((unsigned) rxDataArgs->dataSize, &message_buffer);
        GG_DynamicBuffer_SetData(message_buffer, (const uint8_t *) rxDataArgs->data, (size_t)rxDataArgs->dataSize);

        GG_DataSink_PutData(rxDataArgs->rxSource->sink,
                            GG_DynamicBuffer_AsBuffer(message_buffer), NULL);
        GG_DynamicBuffer_Release(message_buffer);
    } else {
        GG_Log_JNI("RxSource", "Unable to process rx data. Sink is null");
        return GG_ERROR_INVALID_STATE;
    }

    return GG_SUCCESS;
}

extern "C"
JNIEXPORT void
JNICALL
Java_com_fitbit_goldengate_bindings_io_RxSource_receiveData(
        JNIEnv *env,
        jobject /* this */,
        jbyteArray byteArray,
        jlong rxSourcePtr) {
    RxSource *source = (RxSource *) (intptr_t) rxSourcePtr;
    GG_ASSERT(source);
    jbyte *buffer = env->GetByteArrayElements(byteArray, NULL);
    jsize size = env->GetArrayLength(byteArray);

    RxSourceDataArgs args = {
            .rxSource = source,
            .data = buffer,
            .dataSize = size
    };

    GG_Result result;
    GG_Log_JNI("RxSource", "Receiving data on RxSource");
    Loop_InvokeSync(handleReceiveData, &args, &result);
    env->ReleaseByteArrayElements(byteArray, buffer, JNI_ABORT);

}

// Blaster
typedef struct {
    GG_BlasterDataSource* blasterDataSource;
    GG_PerfDataSink* perfDataSink;
    GG_DataSource* stackDataSource;
} BlasterWrapper;

typedef struct {
    GG_DataSink* dataSinkToAttachTo;
    GG_DataSource* dataSourceToAttachTo;
    BlasterWrapper* wrapper;
    size_t packetSize;
    size_t maxPacketCount;
    unsigned int sendInterval;
    GG_BlasterDataSourcePacketFormat packetFormat;
} BlasterInitArgs;

GG_Result attachPerfSinkAndBlaster(void* args) {
    BlasterInitArgs *blasterInitArgs = (BlasterInitArgs *) args;

    GG_Loop* loop = Loop_GetLoop();
    GG_TimerScheduler* scheduler = GG_Loop_GetTimerScheduler(loop);

    BlasterWrapper* blasterWrapper = (BlasterWrapper *)GG_AllocateZeroMemory(sizeof(BlasterWrapper));
    blasterInitArgs->wrapper = blasterWrapper;

    if (blasterInitArgs->dataSinkToAttachTo) {
        GG_Log_JNI("Blaster", "Creating blaster data source");
        GG_Result result = GG_BlasterDataSource_Create(
                blasterInitArgs->packetSize,
                blasterInitArgs->packetFormat,
                blasterInitArgs->maxPacketCount,
                scheduler,
                blasterInitArgs->sendInterval,
                &blasterWrapper->blasterDataSource);
        if (GG_FAILED(result)) {
            GG_Log_JNI("Blaster", "Could not create blaster data source");
            return result;
        } else {
            GG_Log_JNI("Blaster", "setting blaster as data source");
            result = GG_DataSource_SetDataSink(
                    GG_BlasterDataSource_AsDataSource(blasterWrapper->blasterDataSource),
                    blasterInitArgs->dataSinkToAttachTo);
            if (GG_FAILED(result)) {
                GG_Log_JNI("Blaster", "Failed setting blaster as data source");
                return result;
            }
        }
    } else {
        GG_Log_JNI("Blaster", "Skipping setting blaster as DataSource, as there is no DataSink provided");
    }

    if (blasterInitArgs->dataSourceToAttachTo) {
        GG_Result result = GG_PerfDataSink_Create(
                GG_PERF_DATA_SINK_MODE_BASIC_OR_IP_COUNTER,
                GG_PERF_DATA_SINK_OPTION_PRINT_STATS_TO_LOG,
                1000,
                &blasterWrapper->perfDataSink);
        if (GG_FAILED(result)) {
            GG_Log_JNI("Blaster", "Could not create perf data sink");
            return result;
        } else {
            GG_Log_JNI("Blaster", "setting perf sink as data sink");
            result = GG_DataSource_SetDataSink(
                    blasterInitArgs->dataSourceToAttachTo,
                    GG_PerfDataSink_AsDataSink(blasterWrapper->perfDataSink));
            if (GG_FAILED(result)) {
                GG_Log_JNI("Blaster", "Failed perf sink as data sink");
                return result;
            }
            blasterWrapper->stackDataSource = blasterInitArgs->dataSourceToAttachTo;
        }
    } else {
        // this can be the case when we only want to use
        GG_Log_JNI("Blaster", "Skipping setting perf sink as DataSink, as there is no DataSource provided");
    }

    return GG_SUCCESS;
}

JNIEXPORT jlong
JNICALL Java_com_fitbit_goldengate_bindings_io_Blaster_attach(
        JNIEnv *env,
        jobject thiz/* this */,
        jlong dataSinkToAttach,
        jlong dataSourceToAttach,
        jboolean lwipBased,
        jint packetSize,
        jint maxPacketCount,
        jlong sendInterval
) {
    BlasterInitArgs blasterInitArgs = {
        .dataSinkToAttachTo = (GG_DataSink *)(intptr_t) dataSinkToAttach,
        .dataSourceToAttachTo = (GG_DataSource *)(intptr_t) dataSourceToAttach,
        .wrapper = NULL,
        .packetSize = (size_t) packetSize,
        .maxPacketCount = (size_t) maxPacketCount,
        .sendInterval = (unsigned int) sendInterval,
        .packetFormat = (bool)lwipBased ? GG_BLASTER_BASIC_COUNTER_PACKET_FORMAT
                                        : GG_BLASTER_IP_COUNTER_PACKET_FORMAT
    };

    GG_Result result;
    Loop_InvokeSync(attachPerfSinkAndBlaster, &blasterInitArgs, &result);

    BlasterWrapper* blasterWrapper = blasterInitArgs.wrapper;

    return (jlong) (intptr_t) blasterWrapper;
}

static GG_Result startBlaster(void* args) {
    return GG_BlasterDataSource_Start((GG_BlasterDataSource*)args);
}

JNIEXPORT void
JNICALL Java_com_fitbit_goldengate_bindings_io_Blaster_start(
        JNIEnv *env,
        jobject thiz/* this */,
        jlong blasterWrapper) {
    BlasterWrapper* wrapper = (BlasterWrapper *) (intptr_t) blasterWrapper;
    GG_Result result;
    Loop_InvokeSync(startBlaster, wrapper->blasterDataSource, &result);
}

static GG_Result stopBlaster(void* args) {
    return GG_BlasterDataSource_Stop((GG_BlasterDataSource*)args);
}

JNIEXPORT void
JNICALL Java_com_fitbit_goldengate_bindings_io_Blaster_stop(
        JNIEnv *env,
        jobject thiz/* this */,
        jlong blasterWrapperPtr) {
    BlasterWrapper* wrapper = (BlasterWrapper *) (intptr_t) blasterWrapperPtr;
    GG_Result result;
    Loop_InvokeSync(stopBlaster, wrapper->blasterDataSource, &result);
}

JNIEXPORT jobject
JNICALL
Java_com_fitbit_goldengate_bindings_io_Blaster_getStats(
        JNIEnv *env,
        jobject /* this */,
        jlong blasterWrapperPtr,
        jclass clazz) {
    BlasterWrapper* wrapper = (BlasterWrapper  *) (intptr_t) blasterWrapperPtr;
    return getPerfDataSinkStats(env, wrapper->perfDataSink, clazz);
}

static int destroyBlaster(void* args) {
    BlasterWrapper* blasterWrapper = (BlasterWrapper *) args;
    if (blasterWrapper->stackDataSource) {
        GG_DataSource_SetDataSink(blasterWrapper->stackDataSource, NULL);
    }
    GG_BlasterDataSource_Destroy(blasterWrapper->blasterDataSource);
    GG_PerfDataSink_Destroy(blasterWrapper->perfDataSink);
    GG_ClearAndFreeObject(blasterWrapper, 0);
    return GG_SUCCESS;
}

JNIEXPORT void
JNICALL Java_com_fitbit_goldengate_bindings_io_Blaster_destroy(
        JNIEnv *env,
        jobject thiz/* this */,
        jlong blasterWrapperPtr) {
    GG_Result result = 0;
    Loop_InvokeSync(destroyBlaster, (BlasterWrapper *) (intptr_t) blasterWrapperPtr, &result);
}

typedef struct {
    GG_DataSink* dataSinkToAttachTo;
    GG_LoopDataSinkProxy* sinkProxy;
} AttachProxyArgs;

static int attachSinkProxy(void* _args) {
    AttachProxyArgs* args = (AttachProxyArgs *)_args;

    args->sinkProxy = NULL;
    return GG_Loop_CreateDataSinkProxy(Loop_GetLoop(),
                                       GG_LOOP_DATA_SINK_PROXY_MAX_QUEUE_LENGTH,
                                       args->dataSinkToAttachTo,
                                       &args->sinkProxy);
}

JNIEXPORT jlong
JNICALL Java_com_fitbit_goldengate_bindings_io_SingleMessageSender_attach(
        JNIEnv *env,
        jobject thiz/* this */,
        jlong dataSinkToAttach) {

    AttachProxyArgs attachArgs = {
        .dataSinkToAttachTo = (GG_DataSink *) (intptr_t) dataSinkToAttach
    };

    GG_Result result;
    Loop_InvokeSync(attachSinkProxy, &attachArgs, &result);

    return (jlong)(intptr_t)attachArgs.sinkProxy;
}

static int destroySinkProxy(void* args) {
    GG_LoopDataSinkProxy_Destroy((GG_LoopDataSinkProxy *)args);
    return GG_SUCCESS;
}

JNIEXPORT void
JNICALL Java_com_fitbit_goldengate_bindings_io_SingleMessageSender_destroy(
        JNIEnv *env,
        jobject thiz/* this */,
        jlong sinkProxyPtr) {

    GG_Result result = 0;
    Loop_InvokeSync(destroySinkProxy, (void *)(intptr_t)sinkProxyPtr, &result);
}

JNIEXPORT jboolean
JNICALL Java_com_fitbit_goldengate_bindings_io_SingleMessageSender_send(
        JNIEnv *env,
        jobject thiz,
        jlong sinkProxyPtr,
        jbyteArray data) {

    GG_LoopDataSinkProxy* sinkProxy = (GG_LoopDataSinkProxy *)(intptr_t)sinkProxyPtr;

    // check that we have a sink
    if (sinkProxy == NULL) {
        GG_Log_JNI("SingleMessageSender", "Unable to send single message. sinkProxy is null");
        return JNI_FALSE;
    }

    jbyte *buffer = env->GetByteArrayElements(data, NULL);
    int size = env->GetArrayLength(data);

    GG_DynamicBuffer *message_buffer;
    GG_DynamicBuffer_Create((unsigned) size, &message_buffer);
    GG_DynamicBuffer_SetData(message_buffer, (const uint8_t *) buffer, size);

    GG_Log_JNI("SingleMessageSender", "Sending data");
    GG_Result result;
    result = GG_DataSink_PutData(GG_LoopDataSinkProxy_AsDataSink(sinkProxy),
                                 GG_DynamicBuffer_AsBuffer(message_buffer), NULL);

    GG_DynamicBuffer_Release(message_buffer);
    env->ReleaseByteArrayElements(data, buffer, JNI_ABORT);

    return (jboolean) GG_SUCCEEDED(result);
}

}

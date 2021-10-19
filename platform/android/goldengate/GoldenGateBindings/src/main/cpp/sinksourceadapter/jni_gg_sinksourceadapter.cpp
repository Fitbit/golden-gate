#include "platform/android/goldengate/GoldenGateBindings/src/main/cpp/sinksourceadapter/jni_gg_sinksourceadapter.h"
#include "platform/android/goldengate/GoldenGateBindings/src/main/cpp/io/jni_gg_io.h"
#include "platform/android/goldengate/GoldenGateBindings/src/main/cpp/jni_gg_loop.h"
#include "platform/android/goldengate/GoldenGateBindings/src/main/cpp/logging/jni_gg_logging.h"
#include <xp/common/gg_io.h>

extern "C" {

typedef struct {
    RxSource *rx_source;
    TxSink *tx_sink;
    GG_DataSource *data_source;
    GG_DataSink *data_sink;
} SinkSourceAdapter_AttachArgs;

typedef struct {
    RxSource *rx_source;
    GG_DataSource *data_source;
} SinkSourceAdapter_DetachArgs;

static int SinkSourceAdapter_Attach(void *_args) {
    SinkSourceAdapter_AttachArgs *args = (SinkSourceAdapter_AttachArgs *) _args;

    if (args->rx_source && args->data_sink) {
        GG_DataSink *sink = (GG_DataSink *) (intptr_t) args->data_sink;
        GG_DataSource_SetDataSink(GG_CAST(args->rx_source, GG_DataSource), sink);
    }

    if (args->tx_sink && args->data_source) {
        TxSink *tx_sink = args->tx_sink;
        GG_DataSource *source = (GG_DataSource *) (intptr_t) args->data_source;
        GG_DataSource_SetDataSink(source, GG_CAST(tx_sink, GG_DataSink));
    }

    return GG_SUCCESS;
}

static int SinkSourceAdapter_Detach(void *_args) {
    SinkSourceAdapter_DetachArgs *args = (SinkSourceAdapter_DetachArgs *) _args;

    if (args->rx_source) {
        GG_DataSource_SetDataSink(GG_CAST(args->rx_source, GG_DataSource), nullptr);
    }
    if (args->data_source) {
        GG_DataSource_SetDataSink(args->data_source, nullptr);
    }

    return GG_SUCCESS;
}

JNIEXPORT void JNICALL
Java_com_fitbit_goldengate_bindings_sinksourceadapter_SinkSourceAdapter_attach(
        JNIEnv *env,
        jobject thiz,
        jlong rx_source_ptr,
        jlong tx_sink_ptr,
        jlong source_ptr,
        jlong sink_ptr
) {

    GG_ASSERT(rx_source_ptr);
    GG_ASSERT(tx_sink_ptr);
    GG_ASSERT(source_ptr);
    GG_ASSERT(sink_ptr);

    SinkSourceAdapter_AttachArgs attach_args = {
            .rx_source = (RxSource *) rx_source_ptr,
            .tx_sink = (TxSink *) tx_sink_ptr,
            .data_source = (GG_DataSource *) source_ptr,
            .data_sink = (GG_DataSink *) sink_ptr
    };

    int attach_result = 0;
    Loop_InvokeSync(SinkSourceAdapter_Attach, &attach_args, &attach_result);

    if (GG_FAILED(attach_result)) {
        GG_Log_JNI("SinkSourceAdapter", "SinkSourceAdapter_Attach failed with error code %d", attach_result);
    }
}


JNIEXPORT void JNICALL
Java_com_fitbit_goldengate_bindings_sinksourceadapter_SinkSourceAdapter_detach(
        JNIEnv *env,
        jobject thiz,
        jlong rx_source_ptr,
        jlong source_ptr
) {

    SinkSourceAdapter_DetachArgs detach_args = {
            .rx_source = (RxSource *) rx_source_ptr,
            .data_source = (GG_DataSource *) source_ptr
    };

    int detach_result = 0;
    Loop_InvokeSync(SinkSourceAdapter_Detach, &detach_args, &detach_result);

    if (GG_FAILED(detach_result)) {
        GG_Log_JNI("SinkSourceAdapter", "SinkSourceAdapter_Detach failed with error code %d", detach_result);
    }
}
}

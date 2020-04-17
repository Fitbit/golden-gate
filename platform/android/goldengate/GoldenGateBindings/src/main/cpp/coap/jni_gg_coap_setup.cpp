// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

#include <jni_gg_loop.h>
#include <string.h>
#include <logging/jni_gg_logging.h>
#include <util/jni_gg_utils.h>
#include <xp/loop/gg_loop.h>
#include <xp/coap/gg_coap.h>
#include <xp/common/gg_memory.h>
#include <xp/coap/gg_coap_filters.h>

extern "C" {

typedef struct {
    GG_CoapEndpoint *endpoint;
} CoapEndpoint_CreateArgs;

typedef struct {
    GG_CoapEndpoint *endpoint;
    GG_DataSource *source;
    GG_DataSink *sink;
} CoapEndpoint_AttachArgs;

typedef struct {
    GG_CoapEndpoint *endpoint;
    GG_DataSource *source;
} CoapEndpoint_DetachArgs;

typedef struct {
    GG_CoapEndpoint *endpoint;
    GG_CoapGroupRequestFilter *filter;
} CoapEndpoint_AttachFilterArgs;

static int CoapEndpoint_Create(void *_args) {
    CoapEndpoint_CreateArgs *args = (CoapEndpoint_CreateArgs *) _args;

    return GG_CoapEndpoint_Create(
            Loop_GetTimerScheduler(),
            NULL,
            NULL,
            &args->endpoint);
}

static int CoapEndpoint_Attach(void *_args) {
    CoapEndpoint_AttachArgs *args = (CoapEndpoint_AttachArgs *) _args;

    if (args->endpoint && args->sink) {
        GG_DataSource_SetDataSink(GG_CoapEndpoint_AsDataSource(args->endpoint), args->sink);
    }
    if (args->source && args->endpoint) {
        GG_DataSource_SetDataSink(args->source, GG_CoapEndpoint_AsDataSink(args->endpoint));
    }

    return GG_SUCCESS;
}

static int CoapEndpoint_Detach(void *_args) {
    CoapEndpoint_DetachArgs *args = (CoapEndpoint_DetachArgs *) _args;

    if (args->endpoint) {
        GG_DataSource_SetDataSink(GG_CoapEndpoint_AsDataSource(args->endpoint), nullptr);
    }
    if (args->source) {
        GG_DataSource_SetDataSink(args->source, nullptr);
    }

    return GG_SUCCESS;
}

static int CoapEndpoint_AttachFilter(void *_args) {
    CoapEndpoint_AttachFilterArgs *args = (CoapEndpoint_AttachFilterArgs *) _args;

    if ((args->filter) && (args->endpoint)) {
        GG_CoapEndpoint_RegisterRequestFilter(
                args->endpoint,
                GG_CoapGroupRequestFilter_AsCoapRequestFilter(args->filter));
    }

    return GG_SUCCESS;
}

static void CoapEndpoint_Destroy(void *_args) {
    GG_CoapEndpoint *endpoint = (GG_CoapEndpoint *) _args;
    GG_CoapEndpoint_Destroy(endpoint);
}

JNIEXPORT jlong JNICALL
Java_com_fitbit_goldengate_bindings_coap_CoapEndpoint_create(
        JNIEnv *env,
        jobject thiz
) {
    CoapEndpoint_CreateArgs create_args;

    int create_result = 0;
    Loop_InvokeSync(CoapEndpoint_Create, &create_args, &create_result);

    if (GG_FAILED(create_result)) {
        GG_Log_JNI("CoapEndpoint", "CoapEndpoint_Create failed with error code %d", create_result);
        return create_result;
    }

    return (jlong) (intptr_t) create_args.endpoint;
}

JNIEXPORT void JNICALL
Java_com_fitbit_goldengate_bindings_coap_CoapEndpoint_attach(
        JNIEnv *env,
        jobject thiz,
        jlong _endpoint,
        jlong _sourcePtr,
        jlong _sinkPtr
) {
    GG_CoapEndpoint *endpoint = (GG_CoapEndpoint *) (intptr_t) _endpoint;
    GG_ASSERT(endpoint);
    GG_ASSERT(_sourcePtr);
    GG_ASSERT(_sinkPtr);

    CoapEndpoint_AttachArgs attach_args = {
            .endpoint = endpoint,
            .source = (GG_DataSource *) _sourcePtr,
            .sink = (GG_DataSink *) _sinkPtr
    };

    int attach_result = 0;
    Loop_InvokeSync(CoapEndpoint_Attach, &attach_args, &attach_result);

    if (GG_FAILED(attach_result)) {
        GG_Log_JNI("CoapEndpoint", "CoapEndpoint_Attach failed with error code %d", attach_result);
    }
}

JNIEXPORT void JNICALL
Java_com_fitbit_goldengate_bindings_coap_CoapEndpoint_detach(
        JNIEnv *env,
        jobject thiz,
        jlong _endpoint,
        jlong _sourcePtr
) {
    GG_CoapEndpoint *endpoint = (GG_CoapEndpoint *) (intptr_t) _endpoint;
    GG_ASSERT(endpoint);

    CoapEndpoint_DetachArgs detach_args = {
            .endpoint = endpoint,
            .source = (GG_DataSource *) _sourcePtr
    };

    int detach_result = 0;
    Loop_InvokeSync(CoapEndpoint_Detach, &detach_args, &detach_result);

    if (GG_FAILED(detach_result)) {
        GG_Log_JNI("CoapEndpoint", "CoapEndpoint_Detach failed with error code %d", detach_result);
    }
}

JNIEXPORT void JNICALL
Java_com_fitbit_goldengate_bindings_coap_CoapEndpoint_attachFilter(
        JNIEnv *env,
        jobject thiz,
        jlong _endpoint,
        jlong _filter
) {
    GG_CoapEndpoint *endpoint = (GG_CoapEndpoint *) (intptr_t) _endpoint;
    GG_ASSERT(endpoint);
    GG_CoapGroupRequestFilter *filter = (GG_CoapGroupRequestFilter *) (intptr_t) _filter;
    GG_ASSERT(filter);

    CoapEndpoint_AttachFilterArgs attachFilter_args = {
            .endpoint = endpoint,
            .filter = filter
    };

    int create_result = 0;
    Loop_InvokeSync(CoapEndpoint_AttachFilter, &attachFilter_args, &create_result);

    if (GG_FAILED(create_result)) {
        GG_Log_JNI("CoapEndpoint", "CoapEndpoint_AttachFilter failed with error code %d", create_result);
    }
}

JNIEXPORT jlong JNICALL
Java_com_fitbit_goldengate_bindings_coap_CoapEndpoint_asDataSource(
        JNIEnv *env,
        jobject thiz,
        jlong endpoint
) {
    GG_ASSERT(endpoint);

    return (jlong) (intptr_t) GG_CoapEndpoint_AsDataSource((GG_CoapEndpoint*)endpoint);
}

JNIEXPORT jlong JNICALL
Java_com_fitbit_goldengate_bindings_coap_CoapEndpoint_asDataSink(
        JNIEnv *env,
        jobject thiz,
        jlong endpoint
) {
    GG_ASSERT(endpoint);

    return (jlong) (intptr_t) GG_CoapEndpoint_AsDataSink((GG_CoapEndpoint*)endpoint);
}

JNIEXPORT void JNICALL
Java_com_fitbit_goldengate_bindings_coap_CoapEndpoint_destroy(
        JNIEnv *env,
        jobject thiz,
        jlong _endpoint
) {
    GG_CoapEndpoint *endpoint = (GG_CoapEndpoint *) (intptr_t) _endpoint;
    GG_ASSERT(endpoint);

    Loop_InvokeAsync(CoapEndpoint_Destroy, endpoint);
}

}

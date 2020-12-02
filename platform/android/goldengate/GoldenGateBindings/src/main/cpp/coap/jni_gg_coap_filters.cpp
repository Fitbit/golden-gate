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
#include <util/jni_gg_native_reference.h>

extern "C" {

// class names
#define COAP_GROUP_REQUEST_FILTER_CLASS_NAME "com/fitbit/goldengate/bindings/coap/CoapGroupRequestFilter"

typedef struct {
    GG_CoapGroupRequestFilter *filter;
} CoapGroupRequestFilter_CreateArgs;

typedef struct {
    GG_CoapGroupRequestFilter *filter;
    uint8_t group;
} CoapGroupRequestFilter_SetGroupArgs;

static int CoapGroupRequestFilter_Create(void *_args) {
    CoapGroupRequestFilter_CreateArgs *args = (CoapGroupRequestFilter_CreateArgs *) _args;

    return GG_CoapGroupRequestFilter_Create(&args->filter);
}

static int CoapGroupRequestFilter_SetGroup(void *_args) {
    CoapGroupRequestFilter_SetGroupArgs *args = (CoapGroupRequestFilter_SetGroupArgs *) _args;

    return GG_CoapGroupRequestFilter_SetGroup(args->filter, args->group);
}

static void CoapGroupRequestFilter_Destroy(void *_args) {
    NativeReferenceWrapper *filterWrapper = (NativeReferenceWrapper *) _args;

    //we're running on the GG Loop thread
    JNIEnv* env = Loop_GetJNIEnv();

    callJavaObjectOnFreeMethod(
            env,
            COAP_GROUP_REQUEST_FILTER_CLASS_NAME,
            filterWrapper->java_object);

    env->DeleteGlobalRef(filterWrapper->java_object);

    GG_CoapGroupRequestFilter_Destroy((GG_CoapGroupRequestFilter*)filterWrapper->pointer);
    GG_FreeMemory(filterWrapper);
}

JNIEXPORT jlong JNICALL
Java_com_fitbit_goldengate_bindings_coap_CoapGroupRequestFilter_create(
        JNIEnv *env,
        jobject thiz
) {
    CoapGroupRequestFilter_CreateArgs create_args;

    int create_result = 0;
    Loop_InvokeSync(CoapGroupRequestFilter_Create, &create_args, &create_result);

    if (GG_FAILED(create_result)) {
        GG_Log_JNI("CoapGroupRequestFilter", "CoapGroupRequestFilter_Create failed with error code %d", create_result);
        return create_result;
    }

    NativeReferenceWrapper* wrapper = (NativeReferenceWrapper*) GG_AllocateMemory(sizeof(NativeReferenceWrapper));
    wrapper->pointer = create_args.filter;
    wrapper->java_object = env->NewGlobalRef(thiz);

    return (jlong) (intptr_t) wrapper;
}

JNIEXPORT void JNICALL
Java_com_fitbit_goldengate_bindings_coap_CoapGroupRequestFilter_setGroup(
        JNIEnv *env,
        jobject thiz,
        jlong _filter_wrapper,
        jbyte _group
) {
    NativeReferenceWrapper *filter_wrapper = (NativeReferenceWrapper *) (intptr_t) _filter_wrapper;
    if (!filter_wrapper || !filter_wrapper->pointer) {
        return;
    }

    CoapGroupRequestFilter_SetGroupArgs setGroup_args = (CoapGroupRequestFilter_SetGroupArgs) {
                .filter = (GG_CoapGroupRequestFilter *)filter_wrapper->pointer,
                .group = (uint8_t) _group
        };

    GG_ASSERT(setGroup_args.filter);
    GG_ASSERT(setGroup_args.group >= 0 &&
              setGroup_args.group <= GG_COAP_GROUP_REQUEST_FILTER_MAX_GROUP);

    int result = 0;
    Loop_InvokeSync(CoapGroupRequestFilter_SetGroup, &setGroup_args, &result);

    if (GG_FAILED(result)) {
        GG_Log_JNI("CoapGroupRequestFilter", "CoapGroupRequestFilter_Create failed with error code %d", result);
    }
}

JNIEXPORT void JNICALL
Java_com_fitbit_goldengate_bindings_coap_CoapGroupRequestFilter_destroy(
        JNIEnv *env,
        jobject thiz,
        jlong _filter_wrapper
) {
    NativeReferenceWrapper *filter_wrapper = (NativeReferenceWrapper *) (intptr_t) _filter_wrapper;
    if (!filter_wrapper || !filter_wrapper->pointer) {
        return;
    }

    Loop_InvokeAsync(CoapGroupRequestFilter_Destroy, filter_wrapper);
}

}

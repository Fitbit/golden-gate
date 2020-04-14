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
    GG_CoapGroupRequestFilter *filter = (GG_CoapGroupRequestFilter *) _args;
    GG_CoapGroupRequestFilter_Destroy(filter);
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

    return (jlong) (intptr_t) create_args.filter;
}

JNIEXPORT void JNICALL
Java_com_fitbit_goldengate_bindings_coap_CoapGroupRequestFilter_setGroup(
        JNIEnv *env,
        jobject thiz,
        jlong _filter,
        jbyte _group
) {
    CoapGroupRequestFilter_SetGroupArgs setGroup_args = (CoapGroupRequestFilter_SetGroupArgs) {
                .filter = (GG_CoapGroupRequestFilter *) (intptr_t)_filter,
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
        jlong _filter
) {
    GG_CoapGroupRequestFilter *filter = (GG_CoapGroupRequestFilter *) (intptr_t) _filter;
    GG_ASSERT(filter);

    Loop_InvokeAsync(CoapGroupRequestFilter_Destroy, filter);
}

}

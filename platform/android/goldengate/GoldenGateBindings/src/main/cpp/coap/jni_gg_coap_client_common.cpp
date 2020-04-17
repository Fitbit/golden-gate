// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

#include "jni_gg_coap_client_common.h"
#include <jni.h>
#include <jni_gg_loop.h>
#include <string.h>
#include <stdlib.h>
#include <util/jni_gg_utils.h>
#include <xp/loop/gg_loop.h>
#include <xp/coap/gg_coap.h>
#include <xp/common/gg_memory.h>
#include "../logging/jni_gg_logging.h"
#include <xp/common/gg_strings.h>

extern "C" {

void CoapEndpoint_OnAck_Caller(jobject listener) {
    GG_ASSERT(listener);

    JNIEnv *env = Loop_GetJNIEnv();
    jclass listener_class = env->FindClass(COAP_RESPONSE_LISTENER_CLASS_NAME);
    GG_ASSERT(listener_class);
    jmethodID on_ack_method_id = env->GetMethodID(
            listener_class,
            COAP_RESPONSE_LISTENER_ON_ACK_NAME,
            COAP_RESPONSE_LISTENER_ON_ACK_SIG);
    GG_ASSERT(on_ack_method_id);

    env->CallVoidMethod(listener, on_ack_method_id);

    env->DeleteLocalRef(listener_class);
}

void CoapEndpoint_OnError_Caller(
        JNIEnv *env,
        jobject listener,
        GG_Result error,
        const char *message
) {
    GG_ASSERT(listener);

    jclass listener_class = env->FindClass(COAP_RESPONSE_LISTENER_CLASS_NAME);
    GG_ASSERT(listener_class);
    jmethodID on_error_method_id = env->GetMethodID(
            listener_class,
            COAP_RESPONSE_LISTENER_ON_ERROR_NAME,
            COAP_RESPONSE_LISTENER_ON_ERROR_SIG);
    GG_ASSERT(on_error_method_id);

    jstring error_message = env->NewStringUTF(message ? message : "");
    env->CallVoidMethod(listener, on_error_method_id, (jint) error, error_message);

    env->DeleteLocalRef(listener_class);
    env->DeleteLocalRef(error_message);
}

void CoapEndpoint_OnNext_Caller(
        jobject listener,
        GG_CoapMessage *response
) {
    GG_ASSERT(listener);
    GG_ASSERT(response);

    JNIEnv *env = Loop_GetJNIEnv();

    jobject raw_response_message_object = CoapEndpoint_RawResponseMessage_Object_From_GG_CoapMessage(response);
    GG_ASSERT(raw_response_message_object);

    jclass listener_class = env->FindClass(COAP_RESPONSE_LISTENER_CLASS_NAME);
    GG_ASSERT(listener_class);

    jmethodID on_next_method_id = env->GetMethodID(
            listener_class,
            COAP_RESPONSE_LISTENER_ON_NEXT_NAME,
            COAP_RESPONSE_LISTENER_ON_NEXT_SIG);
    GG_ASSERT(on_next_method_id);

    env->CallVoidMethod(listener, on_next_method_id, raw_response_message_object);

    env->DeleteLocalRef(listener_class);
    env->DeleteLocalRef(raw_response_message_object);
}

void CoapEndpoint_OnComplete_Caller(jobject listener) {
    GG_ASSERT(listener);

    JNIEnv *env = Loop_GetJNIEnv();
    jclass listener_class = env->FindClass(COAP_RESPONSE_LISTENER_CLASS_NAME);
    GG_ASSERT(listener_class);
    jmethodID on_complete_method_id = env->GetMethodID(
            listener_class,
            COAP_RESPONSE_LISTENER_ON_COMPLETE_NAME,
            COAP_RESPONSE_LISTENER_ON_COMPLETE_SIG);
    GG_ASSERT(on_complete_method_id);

    env->CallVoidMethod(listener, on_complete_method_id);

    env->DeleteLocalRef(listener_class);
}

jobject CoapEndpoint_ResponseForResult_Object_From_Values(
        JNIEnv *env,
        GG_Result result,
        void *_response_listener
) {
    jclass response_for_result_class = env->FindClass(
            COAP_RESPONSE_FOR_RESULT_CLASS_NAME);
    GG_ASSERT(response_for_result_class);

    jmethodID response_for_result_constructor_id = env->GetMethodID(
            response_for_result_class,
            CONSTRUCTOR_NAME,
            COAP_RESPONSE_FOR_RESULT_CONSTRUCTOR_SIG);
    GG_ASSERT(response_for_result_constructor_id);

    jint result_code = (jint) result;
    jlong response_listener = (jlong) (intptr_t) _response_listener;

    jobject response_for_result_object = env->NewObject(
            response_for_result_class,
            response_for_result_constructor_id,
            result_code,
            response_listener);
    GG_ASSERT(response_for_result_object);

    env->DeleteLocalRef(response_for_result_class);

    return response_for_result_object;
}

}

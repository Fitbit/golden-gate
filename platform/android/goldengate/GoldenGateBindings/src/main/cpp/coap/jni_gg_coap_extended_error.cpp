// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

#include "jni_gg_coap_common.h"
#include <logging/jni_gg_logging.h>
#include <xp/coap/gg_coap.h>

extern "C" {

#define EXTENDED_ERROR_CLASS_NAME "com/fitbit/goldengate/bindings/coap/data/ExtendedError"
#define EXTENDED_ERROR_CONSTRUCTOR_SIG "(L" JAVA_STRING_CLASS_NAME ";IL" JAVA_STRING_CLASS_NAME ";)V"

/**
 * Create a [ExtendedError] from native [GG_CoapExtendedError]
 */
static jobject
ExtendedError_Object_From_GG_CoapExtendedError(JNIEnv *env, GG_CoapExtendedError error) {
    jclass extended_error_class = env->FindClass(EXTENDED_ERROR_CLASS_NAME);
    GG_ASSERT(extended_error_class);
    jmethodID extended_error_constructor_id = env->GetMethodID(
            extended_error_class,
            CONSTRUCTOR_NAME,
            EXTENDED_ERROR_CONSTRUCTOR_SIG);
    GG_ASSERT(extended_error_constructor_id);

    jstring name_space = NULL;
    if (error.name_space) {
        name_space = Jstring_From_NonNull_Terminated_String(
                env,
                error.name_space,
                error.name_space_size);
    }

    jstring message = NULL;
    if (error.message) {
        message = Jstring_From_NonNull_Terminated_String(
                env,
                error.message,
                error.message_size);
    }

    jobject extended_error_object = env->NewObject(
            extended_error_class,
            extended_error_constructor_id,
            name_space,
            error.code,
            message);
    GG_ASSERT(extended_error_object);

    env->DeleteLocalRef(extended_error_class);
    if (name_space) {
        env->DeleteLocalRef(name_space);
    }
    if (message) {
        env->DeleteLocalRef(message);
    }

    return extended_error_object;
}

JNIEXPORT jobject
JNICALL
Java_com_fitbit_goldengate_bindings_coap_data_ExtendedErrorDecoder_decode(
        JNIEnv *env,
        jobject thiz,
        jbyteArray body
) {
    jbyte *body_buffer = env->GetByteArrayElements(body, NULL);
    int body_size = env->GetArrayLength(body);

    GG_CoapExtendedError error;
    GG_Result result = GG_CoapExtendedError_Decode(
            &error,
            (const uint8_t *) body_buffer,
            (size_t) body_size);

    if (GG_FAILED(result)) {
        GG_Log_JNI("ExtendedErrorDecoder", "Failed to decode");
    }

    jobject decoded_object = ExtendedError_Object_From_GG_CoapExtendedError(env, error);
    env->ReleaseByteArrayElements(body, body_buffer, JNI_ABORT);
    return decoded_object;
}

}

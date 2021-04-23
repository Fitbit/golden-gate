// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

#include "jni_gg_coap_common.h"
#include <jni.h>
#include <jni_gg_loop.h>
#include <string.h>
#include <stdlib.h>
#include <util/jni_gg_utils.h>
#include <xp/loop/gg_loop.h>
#include <xp/coap/gg_coap.h>
#include <xp/common/gg_memory.h>
#include <xp/common/gg_strings.h>
#include <platform/android/goldengate/GoldenGateBindings/src/main/cpp/logging/jni_gg_logging.h>

extern "C" {

GG_CoapMethod CoapEndpoint_GG_CoapMethod_From_Request_Object(JNIEnv *env, jobject request) {
    GG_ASSERT(request);

    jclass request_class = env->FindClass(COAP_BASE_REQUEST_CLASS_NAME);
    GG_ASSERT(request_class);
    jclass method_class = env->FindClass(COAP_METHOD_CLASS_NAME);
    GG_ASSERT(method_class);

    jmethodID method_value_id = env->GetMethodID(
            method_class,
            COAP_GET_VALUE_NAME,
            COAP_METHOD_GET_VALUE_SIG);
    GG_ASSERT(method_value_id);
    jmethodID request_get_method_id = env->GetMethodID(
            request_class,
            COAP_REQUEST_GET_METHOD_NAME,
            COAP_REQUEST_GET_METHOD_SIG);
    GG_ASSERT(request_get_method_id);

    jobject method_object = env->CallObjectMethod(request, request_get_method_id);
    GG_ASSERT(method_object);
    jbyte method = env->CallByteMethod(method_object, method_value_id);

    env->DeleteLocalRef(request_class);
    env->DeleteLocalRef(method_class);
    env->DeleteLocalRef(method_object);

    return (GG_CoapMethod) method;
}

/**
 * Read data bytes from given [BytesArrayOutgoingBody] object
 */
static jbyteArray CoapEndpoint_Body_ByteArray_From_BytesArrayOutgoingBody_Object(
        jobject outgoing_body_object
) {
    GG_ASSERT(outgoing_body_object);

    JNIEnv *env = Loop_GetJNIEnv();
    jclass byte_array_outgoing_body_class = env->FindClass(
            COAP_BYTE_ARRAY_OUTGOING_BODY_CLASS_NAME);
    GG_ASSERT(byte_array_outgoing_body_class);

    jmethodID outgoing_body_data_bytes_id = env->GetMethodID(
            byte_array_outgoing_body_class,
            COAP_OUTGOING_BODY_GET_DATA_NAME,
            COAP_BYTE_ARRAY_OUTGOING_BODY_GET_DATA_SIG);
    GG_ASSERT(outgoing_body_data_bytes_id);

    jbyteArray body_byte_array = (jbyteArray) env->CallObjectMethod(
            outgoing_body_object,
            outgoing_body_data_bytes_id);
    GG_ASSERT(body_byte_array);

    env->DeleteLocalRef(byte_array_outgoing_body_class);

    return body_byte_array;
}

/**
 * Read data bytes from given [OutgoingBody] object
 */
static jbyteArray CoapEndpoint_Body_ByteArray_From_OutgoingBody_Object(
        jobject outgoing_body_object
) {
    GG_ASSERT(outgoing_body_object);

    JNIEnv *env = Loop_GetJNIEnv();
    jclass empty_outgoing_body_class = env->FindClass(
            COAP_EMPTY_OUTGOING_BODY_CLASS_NAME);
    GG_ASSERT(empty_outgoing_body_class);
    jclass byte_array_outgoing_body_class = env->FindClass(
            COAP_BYTE_ARRAY_OUTGOING_BODY_CLASS_NAME);
    GG_ASSERT(byte_array_outgoing_body_class);

    jbyteArray body_byte_array;
    if (env->IsInstanceOf(outgoing_body_object, empty_outgoing_body_class)) {
        body_byte_array = env->NewByteArray(0);
        GG_ASSERT(body_byte_array);
    } else if (env->IsInstanceOf(outgoing_body_object, byte_array_outgoing_body_class)) {
        body_byte_array = CoapEndpoint_Body_ByteArray_From_BytesArrayOutgoingBody_Object(
                outgoing_body_object);
        GG_ASSERT(body_byte_array);
    } else {
        GG_ASSERT(JNI_FALSE && "Reading from this body type not supported");
    }

    env->DeleteLocalRef(empty_outgoing_body_class);
    env->DeleteLocalRef(byte_array_outgoing_body_class);

    return body_byte_array;
}

jbyteArray
CoapEndpoint_Body_ByteArray_From_OutgoingMessage_Object(jobject outgoing_message_object) {
    GG_ASSERT(outgoing_message_object);

    JNIEnv *env = Loop_GetJNIEnv();
    jclass outgoing_message_class = env->FindClass(COAP_OUTGOING_MESSAGE_CLASS_NAME);
    GG_ASSERT(outgoing_message_class);
    jclass byte_array_outgoing_body_class = env->FindClass(
            COAP_BYTE_ARRAY_OUTGOING_BODY_CLASS_NAME);
    GG_ASSERT(byte_array_outgoing_body_class);

    jmethodID outgoing_body_data_bytes_id = env->GetMethodID(
            byte_array_outgoing_body_class,
            COAP_OUTGOING_BODY_GET_DATA_NAME,
            COAP_BYTE_ARRAY_OUTGOING_BODY_GET_DATA_SIG);
    GG_ASSERT(outgoing_body_data_bytes_id);
    jmethodID outgoing_message_get_body_id = env->GetMethodID(
            outgoing_message_class,
            COAP_OUTGOING_MESSAGE_GET_BODY_NAME,
            COAP_OUTGOING_MESSAGE_GET_BODY_SIG);
    GG_ASSERT(outgoing_message_get_body_id);

    jobject outgoing_body_object = env->CallObjectMethod(
            outgoing_message_object,
            outgoing_message_get_body_id);
    GG_ASSERT(outgoing_body_object);
    jbyteArray body_byte_array = CoapEndpoint_Body_ByteArray_From_OutgoingBody_Object(
            outgoing_body_object);

    env->DeleteLocalRef(outgoing_message_class);
    env->DeleteLocalRef(byte_array_outgoing_body_class);
    env->DeleteLocalRef(outgoing_body_object);

    return body_byte_array;
}

unsigned int CoapEndpoint_OptionSize_From_Message_Object(JNIEnv *env, jobject message) {
    GG_ASSERT(message);

    jclass list_class = env->FindClass(JAVA_LIST_CLASS_NAME);
    GG_ASSERT(list_class);
    jclass message_class = env->FindClass(COAP_MESSAGE_CLASS_NAME);
    GG_ASSERT(message_class);

    jmethodID list_size_id = env->GetMethodID(
            list_class,
            JAVA_LIST_SIZE_NAME,
            JAVA_LIST_SIZE_SIG);
    GG_ASSERT(list_size_id);
    jmethodID message_get_options_id = env->GetMethodID(
            message_class,
            COAP_MESSAGE_GET_OPTIONS_NAME,
            COAP_MESSAGE_GET_OPTIONS_SIG);
    GG_ASSERT(message_get_options_id);

    jobject options_object = env->CallObjectMethod(message, message_get_options_id);
    GG_ASSERT(options_object);

    unsigned int options_count = (unsigned int) env->CallIntMethod(options_object, list_size_id);

    env->DeleteLocalRef(list_class);
    env->DeleteLocalRef(message_class);
    env->DeleteLocalRef(options_object);

    return options_count;
}

static GG_CoapMessageOptionType CoapEndpoint_GG_CoapMessageOptionType_From_Value_Object(
        JNIEnv *env,
        jobject option_value_object
) {
    GG_ASSERT(option_value_object);

    jclass int_option_value_class = env->FindClass(COAP_INT_OPTION_VALUE_CLASS_NAME);
    GG_ASSERT(int_option_value_class);
    jclass string_option_value_class = env->FindClass(COAP_STRING_OPTION_VALUE_CLASS_NAME);
    GG_ASSERT(string_option_value_class);
    jclass opaque_option_value_class = env->FindClass(COAP_OPAQUE_OPTION_VALUE_CLASS_NAME);
    GG_ASSERT(opaque_option_value_class);
    jclass empty_option_value_class = env->FindClass(COAP_EMPTY_OPTION_VALUE_CLASS_NAME);
    GG_ASSERT(empty_option_value_class);

    GG_CoapMessageOptionType type;
    if (env->IsInstanceOf(option_value_object, int_option_value_class)) {
        type = GG_COAP_MESSAGE_OPTION_TYPE_UINT;
    } else if (env->IsInstanceOf(option_value_object, string_option_value_class)) {
        type = GG_COAP_MESSAGE_OPTION_TYPE_STRING;
    } else if (env->IsInstanceOf(option_value_object, opaque_option_value_class)) {
        type = GG_COAP_MESSAGE_OPTION_TYPE_OPAQUE;
    } else if (env->IsInstanceOf(option_value_object, empty_option_value_class)) {
        type = GG_COAP_MESSAGE_OPTION_TYPE_EMPTY;
    } else {
        GG_ASSERT(JNI_FALSE && "Coap option type not supported");
    }

    env->DeleteLocalRef(int_option_value_class);
    env->DeleteLocalRef(string_option_value_class);
    env->DeleteLocalRef(opaque_option_value_class);
    env->DeleteLocalRef(empty_option_value_class);

    return type;
}

static GG_CoapMessageOption CoapEndpoint_GG_CoapMessageOption_Int_From_Values(
        JNIEnv *env,
        jshort option_number,
        jobject option_value_object
) {
    GG_ASSERT(option_value_object);

    jclass int_option_value_class = env->FindClass(COAP_INT_OPTION_VALUE_CLASS_NAME);
    GG_ASSERT(int_option_value_class);

    jmethodID int_value_id = env->GetMethodID(
            int_option_value_class,
            COAP_GET_VALUE_NAME,
            COAP_INT_OPTION_GET_VALUE_SIG);

    jint option_value = env->CallIntMethod(option_value_object, int_value_id);

    GG_CoapMessageOption option = GG_CoapMessageOption {
            .number = (uint32_t) option_number,
            .type = GG_COAP_MESSAGE_OPTION_TYPE_UINT,
            .value = {
                    .uint = (uint32_t) option_value
            }
    };

    env->DeleteLocalRef(int_option_value_class);

    return option;
}

static GG_CoapMessageOption CoapEndpoint_GG_CoapMessageOption_String_From_Values(
        JNIEnv *env,
        jshort option_number,
        jobject option_value_object
) {
    GG_ASSERT(option_value_object);

    jclass string_option_value_class = env->FindClass(COAP_STRING_OPTION_VALUE_CLASS_NAME);
    GG_ASSERT(string_option_value_class);

    jmethodID string_value_id = env->GetMethodID(
            string_option_value_class,
            COAP_GET_VALUE_NAME,
            COAP_STRING_OPTION_GET_VALUE_SIG);

    jstring option_value = (jstring) env->CallObjectMethod(option_value_object, string_value_id);
    GG_ASSERT(option_value);
    const char *option_value_utf = env->GetStringUTFChars(option_value, NULL);
    GG_ASSERT(option_value_utf);
    /**
     * Creating a copy so ReleaseStringUTFChars can be called on string allocated with GetStringUTFChars.
     * options param should release this when its is no longer needed (see: CoapEndpoint_ReleaseOptionParam)
     */
    char *option_value_utf_copy = strdup(option_value_utf);
    GG_ASSERT(option_value_utf_copy);
    env->ReleaseStringUTFChars(option_value, option_value_utf);

    GG_CoapMessageOption option = GG_CoapMessageOption {
            .number = (uint32_t) option_number,
            .type = GG_COAP_MESSAGE_OPTION_TYPE_STRING,
            .value = {
                    .string = {
                            .chars = option_value_utf_copy,
                            .length = 0
                    }
            }
    };

    env->DeleteLocalRef(string_option_value_class);
    env->DeleteLocalRef(option_value);

    return option;
}

static GG_CoapMessageOption CoapEndpoint_GG_CoapMessageOption_Opaque_From_Values(
        JNIEnv *env,
        jshort option_number,
        jobject option_value_object
) {
    GG_ASSERT(option_value_object);

    jclass opaque_option_value_class = env->FindClass(COAP_OPAQUE_OPTION_VALUE_CLASS_NAME);
    GG_ASSERT(opaque_option_value_class);

    jmethodID opaque_value_id = env->GetMethodID(
            opaque_option_value_class,
            COAP_GET_VALUE_NAME,
            COAP_OPAQUE_OPTION_GET_VALUE_SIG);

    jbyteArray option_value = (jbyteArray) env->CallObjectMethod(
            option_value_object,
            opaque_value_id);
    GG_ASSERT(option_value);

    int option_value_size = env->GetArrayLength(option_value);

    /**
     * Creating a copy of opaque value, options param should release this when its is no
     * longer needed (see: CoapEndpoint_ReleaseOptionParam)
     */
    jbyte *option_value_byte = (jbyte *) GG_AllocateMemory(option_value_size);
    GG_ASSERT(option_value_byte);
    env->GetByteArrayRegion(
            option_value,
            0,
            option_value_size,
            option_value_byte);

    GG_CoapMessageOption option = GG_CoapMessageOption{
            .number = (uint32_t) option_number,
            .type = GG_COAP_MESSAGE_OPTION_TYPE_OPAQUE,
            .value = {
                    .opaque = {
                            .bytes = (uint8_t *) option_value_byte,
                            .size = (unsigned int) option_value_size
                    }
            }
    };

    env->DeleteLocalRef(opaque_option_value_class);
    env->DeleteLocalRef(option_value);

    return option;
}

static GG_CoapMessageOption CoapEndpoint_GG_CoapMessageOption_Empty_From_Values(
        jshort option_number
) {
    return GG_CoapMessageOption {
            .number = (uint32_t) option_number,
            .type = GG_COAP_MESSAGE_OPTION_TYPE_EMPTY
    };
}

static GG_CoapMessageOption CoapEndpoint_GG_CoapMessageOption_From_Values(
        JNIEnv *env,
        jshort option_number,
        jobject option_value_object
) {
    GG_ASSERT(option_value_object);

    GG_CoapMessageOptionType option_type = CoapEndpoint_GG_CoapMessageOptionType_From_Value_Object(
            env,
            option_value_object);

    switch (option_type) {
        case GG_COAP_MESSAGE_OPTION_TYPE_UINT :
            return CoapEndpoint_GG_CoapMessageOption_Int_From_Values(
                    env,
                    option_number,
                    option_value_object);
        case GG_COAP_MESSAGE_OPTION_TYPE_STRING :
            return CoapEndpoint_GG_CoapMessageOption_String_From_Values(
                    env,
                    option_number,
                    option_value_object);
        case GG_COAP_MESSAGE_OPTION_TYPE_OPAQUE :
            return CoapEndpoint_GG_CoapMessageOption_Opaque_From_Values(
                    env,
                    option_number,
                    option_value_object);
        case GG_COAP_MESSAGE_OPTION_TYPE_EMPTY :
            return CoapEndpoint_GG_CoapMessageOption_Empty_From_Values(option_number);
    }
}

void CoapEndpoint_GG_CoapMessageOptionParam_From_Message_Object(
        JNIEnv *env,
        jobject message,
        GG_CoapMessageOptionParam *options,
        unsigned int options_count
) {
    GG_ASSERT(message);
    GG_ASSERT(options);

    jclass list_class = env->FindClass(JAVA_LIST_CLASS_NAME);
    GG_ASSERT(list_class);
    jclass option_class = env->FindClass(COAP_OPTION_CLASS_NAME);
    GG_ASSERT(option_class);
    jclass message_class = env->FindClass(COAP_MESSAGE_CLASS_NAME);
    GG_ASSERT(message_class);
    jclass option_number_class = env->FindClass(COAP_OPTION_NUMBER_CLASS_NAME);
    GG_ASSERT(option_number_class);

    jmethodID list_get_id = env->GetMethodID(
            list_class,
            JAVA_LIST_GET_NAME,
            JAVA_LIST_GET_SIG);
    GG_ASSERT(list_get_id);
    jmethodID option_get_number_id = env->GetMethodID(
            option_class,
            COAP_OPTION_GET_NUMBER_NAME,
            COAP_OPTION_GET_NUMBER_SIG);
    GG_ASSERT(option_get_number_id);
    jmethodID option_get_value_id = env->GetMethodID(
            option_class,
            COAP_GET_VALUE_NAME,
            COAP_OPTION_GET_VALUE_SIG);
    GG_ASSERT(option_get_value_id);
    jmethodID message_get_options_id = env->GetMethodID(
            message_class,
            COAP_MESSAGE_GET_OPTIONS_NAME,
            COAP_MESSAGE_GET_OPTIONS_SIG);
    GG_ASSERT(message_get_options_id);
    jmethodID option_number_get_value_id = env->GetMethodID(
            option_number_class,
            COAP_GET_VALUE_NAME,
            COAP_OPTIONS_GET_VALUE_SIG);
    GG_ASSERT(option_number_get_value_id);

    jobject options_object = env->CallObjectMethod(message, message_get_options_id);
    GG_ASSERT(options_object);

    for (int i = 0; i < options_count; ++i) {
        jobject option_object = env->CallObjectMethod(options_object, list_get_id, i);
        GG_ASSERT(option_object);
        jobject option_number_object = env->CallObjectMethod(option_object, option_get_number_id);
        GG_ASSERT(option_number_object);
        // Option Value can be NULL for empty option type
        jobject option_value_object = env->CallObjectMethod(option_object, option_get_value_id);
        jshort option_number = env->CallShortMethod(
                option_number_object,
                option_number_get_value_id);
        GG_ASSERT(option_number);

        GG_CoapMessageOption option = CoapEndpoint_GG_CoapMessageOption_From_Values(
                env,
                option_number,
                option_value_object);

        options[i].option = option;
        options[i].next = NULL;

        env->DeleteLocalRef(option_object);
        env->DeleteLocalRef(option_number_object);
        env->DeleteLocalRef(option_value_object);
    }

    env->DeleteLocalRef(list_class);
    env->DeleteLocalRef(option_class);
    env->DeleteLocalRef(message_class);
    env->DeleteLocalRef(option_number_class);
    env->DeleteLocalRef(options_object);
}

void CoapEndpoint_ReleaseOptionParam(
        GG_CoapMessageOptionParam *options,
        unsigned int options_count
) {
    if (options) {
        for (int i = 0; i < options_count; ++i) {
            switch (options[i].option.type) {
                case GG_COAP_MESSAGE_OPTION_TYPE_STRING:
                    GG_FreeMemory((void *) options[i].option.value.string.chars);
                    break;
                case GG_COAP_MESSAGE_OPTION_TYPE_OPAQUE:
                    GG_FreeMemory((void *) options[i].option.value.opaque.bytes);
                    break;
                default:
                    // no need to cleanup
                    break;
            }
        }
    }
}

static jobject CoapEndpoint_Method_Object_From_GG_CoapMessage(const GG_CoapMessage *message) {
    GG_ASSERT(message);

    jint request_method = GG_CoapMessage_GetCode(message);

    JNIEnv *env = Loop_GetJNIEnv();

    jclass method_class = env->FindClass(COAP_METHOD_CLASS_NAME);
    GG_ASSERT(method_class);

    jmethodID method_from_value_id = env->GetStaticMethodID(
            method_class,
            COAP_METHOD_FROM_VALUE_NAME,
            COAP_METHOD_FROM_VALUE_SIG);
    GG_ASSERT(method_from_value_id);

    jobject method_object = env->CallStaticObjectMethod(
            method_class,
            method_from_value_id,
            request_method);
    GG_ASSERT(method_object);

    env->DeleteLocalRef(method_class);

    return method_object;
}

jint CoapEndpoint_GG_CoapMaxResendCount_From_Request_Object(JNIEnv *env, jobject request) {
    GG_ASSERT(request);

    jclass request_class = env->FindClass(COAP_OUTGOING_REQUEST_CLASS_NAME);
    GG_ASSERT(request_class);

    jmethodID request_get_max_resend_count_id = env->GetMethodID(
            request_class,
            COAP_REQUEST_GET_MAX_RESEND_COUNT_NAME,
            COAP_REQUEST_GET_MAX_RESEND_COUNT_SIG);
    GG_ASSERT(request_get_max_resend_count_id);

    int max_resend_count = (int)env->CallIntMethod(request, request_get_max_resend_count_id);

    // if max_resend_count is not a valid number, use the default value
    if (max_resend_count < 0) {
        max_resend_count = GG_COAP_DEFAULT_MAX_RETRANSMIT;
    }

    env->DeleteLocalRef(request_class);

    return max_resend_count;
}

jint CoapEndpoint_GG_CoapAckTimeout_From_Request_Object(JNIEnv *env, jobject request) {
    GG_ASSERT(request);

    jclass request_class = env->FindClass(COAP_OUTGOING_REQUEST_CLASS_NAME);
    GG_ASSERT(request_class);

    jmethodID request_get_ack_timeout_id = env->GetMethodID(
            request_class,
            COAP_REQUEST_GET_ACK_TIMEOUT_NAME,
            COAP_REQUEST_GET_ACK_TIMEOUT_SIG);
    GG_ASSERT(request_get_ack_timeout_id);

    int ack_timeout = (int)env->CallIntMethod(request, request_get_ack_timeout_id);

    // if ack_timeout is not a valid number, set ack_timeout as zero to use the default value
    if (ack_timeout < 0) {
        ack_timeout = 0;
    }

    env->DeleteLocalRef(request_class);

    return ack_timeout;
}

/**
 * Create a object of type [ResponseCode]
 *
 * @param response Coap response message that has been received.
 * @return new [ResponseCode] object, caller must delete this local reference one done with usage
 */
static jobject CoapEndpoint_ResponseCode_Object_From_GG_CoapMessage(GG_CoapMessage *response) {
    GG_ASSERT(response);

    unsigned int response_code = GG_CoapMessage_GetCode(response);
    jbyte response_code_class = (jbyte) GG_COAP_MESSAGE_CODE_CLASS(response_code);
    jbyte response_code_detail = (jbyte) GG_COAP_MESSAGE_CODE_DETAIL(response_code);

    JNIEnv *env = Loop_GetJNIEnv();

    jclass response_code_class_name = env->FindClass(COAP_RESPONSE_CODE_CLASS_NAME);
    GG_ASSERT(response_code_class_name);
    jmethodID response_code_constructor = env->GetMethodID(
            response_code_class_name,
            CONSTRUCTOR_NAME,
            COAP_RESPONSE_CODE_CONSTRUCTOR_SIG);
    GG_ASSERT(response_code_constructor);

    jobject response_code_object = env->NewObject(
            response_code_class_name,
            response_code_constructor,
            response_code_class,
            response_code_detail);
    GG_ASSERT(response_code_object);

    env->DeleteLocalRef(response_code_class_name);

    return response_code_object;
}

/**
 * Create a single response coap message object [RawRequestMessage]
 *
 * @param request_method [Method] object
 * @param options_object request options object
 * @param response_data []
 * @return body byte array
 */
static jobject CoapEndpoint_RawRequestMessage_Object_From_Member_Values(
        jobject request_method,
        jobject options_object,
        jbyteArray response_data
) {
    GG_ASSERT(request_method);
    GG_ASSERT(options_object);
    GG_ASSERT(response_data);

    JNIEnv *env = Loop_GetJNIEnv();
    jclass raw_request_message_class = env->FindClass(COAP_RAW_REQUEST_MESSAGE_CLASS_NAME);
    GG_ASSERT(raw_request_message_class);

    jmethodID raw_request_message_constructor = env->GetMethodID(
            raw_request_message_class,
            CONSTRUCTOR_NAME,
            COAP_RAW_REQUEST_MESSAGE_CONSTRUCTOR_SIG);
    GG_ASSERT(raw_request_message_constructor);

    jobject raw_request_message_object = env->NewObject(
            raw_request_message_class,
            raw_request_message_constructor,
            request_method,
            options_object,
            response_data);
    GG_ASSERT(raw_request_message_object);

    env->DeleteLocalRef(raw_request_message_class);

    return raw_request_message_object;
}

/**
 * Create a single response coap message object [RawResponseMessage]
 *
 * @param response_code_object response code obtained from CoapEndpoint_Create_ResponseCode
 * @param response_data byte array containing body for a block or single coap response
 * @return new [CoapResponseListener.Message] object, caller must delete this local reference one done with usage
 */
static jobject CoapEndpoint_RawResponseMessage_Object_From_Member_Values(
        jobject response_code_object,
        jobject options_object,
        jbyteArray response_data
) {
    GG_ASSERT(response_code_object);
    GG_ASSERT(options_object);
    GG_ASSERT(response_data);

    JNIEnv *env = Loop_GetJNIEnv();
    jclass raw_response_message_class = env->FindClass(COAP_RAW_RESPONSE_MESSAGE_CLASS_NAME);
    GG_ASSERT(raw_response_message_class);

    jmethodID raw_response_message_constructor = env->GetMethodID(
            raw_response_message_class,
            CONSTRUCTOR_NAME,
            COAP_RAW_RESPONSE_MESSAGE_CONSTRUCTOR_SIG);
    GG_ASSERT(raw_response_message_constructor);

    jobject raw_response_message_object = env->NewObject(
            raw_response_message_class,
            raw_response_message_constructor,
            response_code_object,
            options_object,
            response_data);
    GG_ASSERT(raw_response_message_object);

    env->DeleteLocalRef(raw_response_message_class);

    return raw_response_message_object;
}

jbyteArray CoapEndpoint_Body_BytesArray_From_GG_CoapMessage(const GG_CoapMessage *message) {
    GG_ASSERT(message);

    JNIEnv *env = Loop_GetJNIEnv();

    jsize payload_size = (jsize) GG_CoapMessage_GetPayloadSize(message);
    jbyte *payload = (jbyte *) GG_CoapMessage_GetPayload(message);
    jbyteArray response_data = JByteArray_From_DataPointer(env, payload, payload_size);
    GG_ASSERT(response_data);

    return response_data;
}

jobject CoapEndpoint_RawRequestMessage_Object_From_GG_CoapMessage(
        const GG_CoapMessage *request
) {
    GG_ASSERT(request);

    jobject request_method = CoapEndpoint_Method_Object_From_GG_CoapMessage(request);
    GG_ASSERT(request_method);
    jobject options_object = CoapEndpoint_Option_Object_From_GG_CoapMessage(request);
    GG_ASSERT(options_object);
    jbyteArray response_data = CoapEndpoint_Body_BytesArray_From_GG_CoapMessage(request);
    GG_ASSERT(response_data);

    jobject raw_request_object = CoapEndpoint_RawRequestMessage_Object_From_Member_Values(
            request_method,
            options_object,
            response_data);
    GG_ASSERT(raw_request_object);

    JNIEnv *env = Loop_GetJNIEnv();
    env->DeleteLocalRef(request_method);
    env->DeleteLocalRef(options_object);
    env->DeleteLocalRef(response_data);

    return raw_request_object;
}

jobject CoapEndpoint_RawResponseMessage_Object_From_GG_CoapMessage(
        GG_CoapMessage *response
) {
    GG_ASSERT(response);

    jobject response_code_object = CoapEndpoint_ResponseCode_Object_From_GG_CoapMessage(response);
    GG_ASSERT(response_code_object);
    jobject options_object = CoapEndpoint_Option_Object_From_GG_CoapMessage(response);
    GG_ASSERT(options_object);
    jbyteArray response_data = CoapEndpoint_Body_BytesArray_From_GG_CoapMessage(response);
    GG_ASSERT(response_data);

    jobject message_object = CoapEndpoint_RawResponseMessage_Object_From_Member_Values(
            response_code_object,
            options_object,
            response_data);
    GG_ASSERT(message_object);

    JNIEnv *env = Loop_GetJNIEnv();
    env->DeleteLocalRef(response_code_object);
    env->DeleteLocalRef(options_object);
    env->DeleteLocalRef(response_data);

    return message_object;
}

uint8_t CoapEndpoint_ResponseCode_From_Response_Object(jobject response) {
    GG_ASSERT(response);

    JNIEnv *env = Loop_GetJNIEnv();

    jclass response_class = env->FindClass(COAP_BASE_RESPONSE_CLASS_NAME);
    GG_ASSERT(response_class);
    jclass response_code_class_name = env->FindClass(COAP_RESPONSE_CODE_CLASS_NAME);
    GG_ASSERT(response_code_class_name);

    jmethodID get_response_code_id = env->GetMethodID(
            response_class,
            COAP_RESPONSE_GET_RESPONSE_CODE_NAME,
            COAP_RESPONSE_GET_RESPONSE_CODE_SIG);
    GG_ASSERT(get_response_code_id);
    jmethodID get_response_code_class_id = env->GetMethodID(
            response_code_class_name,
            COAP_RESPONSE_CODE_GET_RESPONSE_CLASS_NAME,
            COAP_RESPONSE_CODE_GET_RESPONSE_CLASS_SIG);
    GG_ASSERT(get_response_code_class_id);
    jmethodID get_response_code_detail_id = env->GetMethodID(
            response_code_class_name,
            COAP_RESPONSE_CODE_GET_DETAIL_NAME,
            COAP_RESPONSE_CODE_GET_DETAIL_SIG);
    GG_ASSERT(get_response_code_detail_id);

    jobject response_code_object = env->CallObjectMethod(
            response,
            get_response_code_id);
    GG_ASSERT(get_response_code_id);
    jbyte response_code_class = env->CallByteMethod(
            response_code_object,
            get_response_code_class_id);
    jbyte response_code_detail = env->CallByteMethod(
            response_code_object,
            get_response_code_detail_id);

    jint response_code = (response_code_class * 100) + response_code_detail;

    env->DeleteLocalRef(response_class);
    env->DeleteLocalRef(response_code_class_name);
    env->DeleteLocalRef(response_code_object);

    return (uint8_t) GG_COAP_MESSAGE_CODE(response_code);
}

jobject CoapEndpoint_Option_Object_From_GG_CoapMessage(const GG_CoapMessage *response) {
    GG_ASSERT(response);

    JNIEnv *env = Loop_GetJNIEnv();

    jclass options_builder_class = env->FindClass(COAP_OPTIONS_BUILDER_CLASS_NAME);
    GG_ASSERT(options_builder_class);
    jclass option_number_class = env->FindClass(COAP_OPTION_NUMBER_CLASS_NAME);
    GG_ASSERT(option_number_class);

    jmethodID options_builder_constructor_id = env->GetMethodID(
            options_builder_class,
            CONSTRUCTOR_NAME,
            DEFAULT_CONSTRUCTOR_SIG);
    GG_ASSERT(options_builder_constructor_id);
    jmethodID options_builder_build_id = env->GetMethodID(
            options_builder_class,
            COAP_OPTIONS_BUILDER_BUILD_NAME,
            COAP_OPTIONS_BUILDER_BUILD_SIG);
    GG_ASSERT(options_builder_build_id);
    jmethodID option_builder_empty_id = env->GetMethodID(
            options_builder_class,
            COAP_OPTIONS_BUILDER_OPTION_NAME,
            COAP_OPTIONS_BUILDER_OPTION_EMPTY_SIG);
    GG_ASSERT(option_builder_empty_id);
    jmethodID option_builder_int_id = env->GetMethodID(
            options_builder_class,
            COAP_OPTIONS_BUILDER_OPTION_NAME,
            COAP_OPTIONS_BUILDER_OPTION_INT_SIG);
    GG_ASSERT(option_builder_int_id);
    jmethodID option_builder_string_id = env->GetMethodID(
            options_builder_class,
            COAP_OPTIONS_BUILDER_OPTION_NAME,
            COAP_OPTIONS_BUILDER_OPTION_STRING_SIG);
    GG_ASSERT(option_builder_string_id);
    jmethodID option_builder_opaque_id = env->GetMethodID(
            options_builder_class,
            COAP_OPTIONS_BUILDER_OPTION_NAME,
            COAP_OPTIONS_BUILDER_OPTION_OPAQUE_SIG);
    GG_ASSERT(option_builder_opaque_id);

    jobject options_builder_object = env->NewObject(
            options_builder_class,
            options_builder_constructor_id);
    GG_ASSERT(options_builder_object);

    GG_CoapMessageOptionIterator option_iterator = GG_CoapMessageOptionIterator();
    GG_CoapMessage_InitOptionIterator(
            response,
            GG_COAP_MESSAGE_OPTION_ITERATOR_FILTER_ANY,
            &option_iterator);

    while (option_iterator.option.number != GG_COAP_MESSAGE_OPTION_NONE) {
        switch (option_iterator.option.type) {
            case GG_COAP_MESSAGE_OPTION_TYPE_UINT: {
                env->CallVoidMethod(
                        options_builder_object,
                        option_builder_int_id,
                        option_iterator.option.number,
                        option_iterator.option.value.uint);
                break;
            }
            case GG_COAP_MESSAGE_OPTION_TYPE_STRING: {
                jstring string_value = Jstring_From_NonNull_Terminated_String(
                        env,
                        option_iterator.option.value.string.chars,
                        option_iterator.option.value.string.length);

                env->CallVoidMethod(
                        options_builder_object,
                        option_builder_string_id,
                        option_iterator.option.number,
                        string_value);

                env->DeleteLocalRef(string_value);

                break;
            }
            case GG_COAP_MESSAGE_OPTION_TYPE_OPAQUE: {
                jsize opaque_value_size = (jsize) option_iterator.option.value.opaque.size;
                jbyteArray opaque_value = env->NewByteArray(opaque_value_size);
                env->SetByteArrayRegion(
                        opaque_value,
                        0,
                        opaque_value_size,
                        (jbyte *) option_iterator.option.value.opaque.bytes);
                GG_ASSERT(opaque_value);

                env->CallVoidMethod(
                        options_builder_object,
                        option_builder_opaque_id,
                        option_iterator.option.number,
                        opaque_value);

                env->DeleteLocalRef(opaque_value);
                break;
            }
            case GG_COAP_MESSAGE_OPTION_TYPE_EMPTY: {
                env->CallVoidMethod(
                        options_builder_object,
                        option_builder_empty_id,
                        option_iterator.option.number);
                break;
            }
            default:
                break;
        }
        GG_CoapMessage_StepOptionIterator(response, &option_iterator);
    }

    jobject options_object = env->CallObjectMethod(
            options_builder_object,
            options_builder_build_id);

    env->DeleteLocalRef(options_builder_class);
    env->DeleteLocalRef(option_number_class);
    env->DeleteLocalRef(options_builder_object);

    return options_object;
}

jboolean CoapEndpoint_AutogenerateBlockwiseConfig_From_Response_Object(jobject response) {
    GG_ASSERT(response);

    JNIEnv *env = Loop_GetJNIEnv();

    jclass response_class = env->FindClass(COAP_OUTGOING_RESPONSE_CLASS_NAME);
    GG_ASSERT(response_class);

    jmethodID get_autogenerate_blockwise_config = env->GetMethodID(
            response_class,
            COAP_RESPONSE_GET_AUTOGENERATE_BLOCKWISE_CONFIG_NAME,
            COAP_RESPONSE_GET_FORCE_NONBLOCKWISE_SIG);
    GG_ASSERT(get_autogenerate_blockwise_config);

    jboolean force_nonblockwise = env->CallBooleanMethod(
            response,
            get_autogenerate_blockwise_config);

    env->DeleteLocalRef(response_class);

    return force_nonblockwise;
}

jstring Jstring_From_NonNull_Terminated_String(
        JNIEnv *env,
        const char *source,
        size_t count
) {
    jstring destination;
    if (count > 0) {
        char _destination[count + 1];
        memcpy(_destination, source, count);
        _destination[count] = '\0';
        destination = env->NewStringUTF(_destination);
    } else {
        destination = env->NewStringUTF("");
    }
    return destination;
}

}

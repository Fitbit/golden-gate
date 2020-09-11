// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

#include "jni_gg_coap_common.h"
#include <jni_gg_loop.h>
#include <string.h>
#include <logging/jni_gg_logging.h>
#include <util/jni_gg_utils.h>
#include <xp/loop/gg_loop.h>
#include <xp/coap/gg_coap.h>
#include <xp/common/gg_memory.h>
#include <xp/coap/gg_coap_filters.h>
#include <xp/coap/gg_coap_blockwise.h>
#include "jni_gg_coap_server.h"
#include "jni_gg_coap_server_block.h"

extern "C" {

/**
 * Helper to free RequestHandler object
 */
static void CoapEndpoint_ResourceHandler_Cleanup(JNIEnv *env, RequestHandler *self) {
    if (self) {
        if (self->response_handler) {
            env->DeleteGlobalRef(self->response_handler);
        }
        GG_ClearAndFreeObject(self, 1);
    }
}

/**
 * Add a resource handler. Must be called from GetLoop thread
 *
 * @param _args object on which this method is invoked.
 */
static GG_Result CoapEndpoint_AddResourceHandler(void *_args) {
    RequestHandler *args = (RequestHandler *) _args;
    GG_ASSERT(args->endpoint);
    GG_ASSERT(args->path);
    GG_ASSERT(args->response_handler);
    GG_ASSERT(args->request_filter_group >= 0 &&
              args->request_filter_group <= GG_COAP_GROUP_REQUEST_FILTER_MAX_GROUP);

    return GG_CoapEndpoint_RegisterRequestHandler(
            args->endpoint,
            args->path,
            static_cast<uint32_t>(GG_COAP_REQUEST_HANDLER_FLAGS_ALLOW_ALL |
                                  GG_COAP_REQUEST_HANDLER_FLAG_GROUP(args->request_filter_group)),
            GG_CAST(args, GG_CoapRequestHandler));
}

/**
 * Remove a resource handler. Must be called from GetLoop thread
 *
 * @param _args object on which this method is invoked.
 */
static GG_Result CoapEndpoint_RemoveResourceHandler(void *_args) {
    RequestHandler *args = (RequestHandler *) _args;
    GG_ASSERT(args->endpoint);

    return GG_CoapEndpoint_UnregisterRequestHandler(
            args->endpoint,
            NULL,
            GG_CAST(args, GG_CoapRequestHandler));
}

/**
 * Helper that will invoke registered response handler on java/kotlin side and return response object
 */
static jobject OutgoingResponse_Object_From_Values(jobject response_handler, jobject raw_request_object) {
    GG_ASSERT(raw_request_object);

    JNIEnv *env = Loop_GetJNIEnv();

    jclass response_invoker_class = env->FindClass(COAP_RESPONSE_HANDLER_INVOKER_CLASS_NAME);
    GG_ASSERT(response_invoker_class);
    jclass raw_response_class = env->FindClass(COAP_RAW_REQUEST_MESSAGE_CLASS_NAME);
    GG_ASSERT(raw_response_class);
    jclass outgoing_response_class = env->FindClass(COAP_OUTGOING_RESPONSE_CLASS_NAME);
    GG_ASSERT(outgoing_response_class);

    jmethodID response_invoker_constructor_id = env->GetMethodID(
            response_invoker_class,
            CONSTRUCTOR_NAME,
            COAP_RESPONSE_HANDLER_INVOKER_CONSTRUCTOR_SIG);
    GG_ASSERT(response_invoker_constructor_id);
    jmethodID response_invoker_invoke_id = env->GetMethodID(
            response_invoker_class,
            COAP_RESPONSE_HANDLER_INVOKER_INVOKE_NAME,
            COAP_RESPONSE_HANDLER_INVOKER_INVOKE_SIG);
    GG_ASSERT(response_invoker_invoke_id);

    jobject response_invoker_object = env->NewObject(
            response_invoker_class,
            response_invoker_constructor_id,
            response_handler);
    GG_ASSERT(response_invoker_object);
    jobject outgoing_response_object = env->CallObjectMethod(
            response_invoker_object,
            response_invoker_invoke_id,
            raw_request_object);
    GG_ASSERT(outgoing_response_object);

    env->DeleteLocalRef(response_invoker_class);
    env->DeleteLocalRef(raw_response_class);
    env->DeleteLocalRef(outgoing_response_class);
    env->DeleteLocalRef(response_invoker_object);

    return outgoing_response_object;
}

/**
 * Method invoked when a request has been received and should be handled by the handler.
 *
 * @param self The object on which this method is invoked.
 * @param endpoint the endpoint that received the request.
 * @param request The request that was received for this handler.
 * @param response Pointer to the variable where the handler should return its response.
 * The handler should only return a response in this variable if it also returns GG_SUCCESS.
 *
 * @return GG_SUCCESS if the handler was able to create a response object, or an error
 * code if it could not.
 */
static GG_Result CoapEndpoint_OnRequest(
        GG_CoapRequestHandler *_self,
        GG_CoapEndpoint *endpoint,
        const GG_CoapMessage *request,
        GG_CoapResponder *responder,
        const GG_BufferMetadata* transport_metadata,
        GG_CoapMessage **response
) {
    GG_COMPILER_UNUSED(responder);
    GG_COMPILER_UNUSED(transport_metadata);

    RequestHandler *self = GG_SELF(RequestHandler, GG_CoapRequestHandler);
    GG_ASSERT(self->response_handler);

    // get the request token
    uint8_t token[GG_COAP_MESSGAGE_MAX_TOKEN_LENGTH];
    size_t token_length = GG_CoapMessage_GetToken(request, token);

    // get block info
    GG_CoapMessageBlockInfo block_info;

    GG_Result result;

    // Delegate to response handler to get the response
    JNIEnv *env = Loop_GetJNIEnv();
    jobject raw_request_object = CoapEndpoint_RawRequestMessage_Object_From_GG_CoapMessage(request);
    GG_ASSERT(raw_request_object);
    jobject outgoing_response_object = OutgoingResponse_Object_From_Values(
            self->response_handler,
            raw_request_object);
    GG_ASSERT(outgoing_response_object);

    uint8_t request_code = GG_CoapMessage_GetCode(request);

    // get autogenerateBlockwiseConfig flag
    jboolean autogenerated = CoapEndpoint_AutogenerateBlockwiseConfig_From_Response_Object(outgoing_response_object);

    if ((request_code == GG_COAP_METHOD_PUT || request_code == GG_COAP_METHOD_POST) && autogenerated){
        GG_CoapMessage_GetBlockInfo(
                request,
                GG_COAP_MESSAGE_OPTION_BLOCK1,
                &block_info,
                GG_COAP_BLOCKWISE_DEFAULT_BLOCK_SIZE);

        // creates coap response with Block1 option
        result = CoapEndpoint_CreateBlockwiseResponseWithServerHelper(
                endpoint,
                self,
                request,
                response);

    } else if (request_code == GG_COAP_METHOD_GET && autogenerated)  {
        GG_CoapMessage_GetBlockInfo(
                request,
                GG_COAP_MESSAGE_OPTION_BLOCK2,
                &block_info,
                GG_COAP_BLOCKWISE_DEFAULT_BLOCK_SIZE);

        // creates coap response with Block2 option
        result = CoapEndpoint_CreateBlockwiseResponseFromBlockSource(
                env,
                endpoint,
                self,
                outgoing_response_object,
                request,
                &block_info,
                response);

    } else {
        // get options params
        unsigned int options_count = CoapEndpoint_OptionSize_From_Message_Object(env,
                                                                                 outgoing_response_object);
        GG_CoapMessageOptionParam options[options_count];
        CoapEndpoint_GG_CoapMessageOptionParam_From_Message_Object(env, outgoing_response_object,
                                                                   options, options_count);

        // get response code
        uint8_t response_code = CoapEndpoint_ResponseCode_From_Response_Object(outgoing_response_object);

        // get payload
        jbyteArray body_byte_array = CoapEndpoint_Body_ByteArray_From_OutgoingMessage_Object(
                outgoing_response_object);
        GG_ASSERT(body_byte_array);
        jbyte *payload = env->GetByteArrayElements(body_byte_array, NULL);
        GG_ASSERT(payload);
        int payload_size = env->GetArrayLength(body_byte_array);

        // create response GG_CoapMessage
        result = GG_CoapMessage_Create(
                response_code,
                GG_COAP_MESSAGE_TYPE_ACK,
                options,
                options_count,
                GG_CoapMessage_GetMessageId(request),
                token,
                token_length,
                (const uint8_t *) payload,
                (unsigned int) payload_size,
                response);

        env->ReleaseByteArrayElements(body_byte_array, payload, JNI_ABORT);
        env->DeleteLocalRef(body_byte_array);
        CoapEndpoint_ReleaseOptionParam(options, options_count);
    }

    env->DeleteLocalRef(raw_request_object);
    env->DeleteLocalRef(outgoing_response_object);


    return result;
}

// Single response Implementation for GG_CoapResponseListener interface
GG_IMPLEMENT_INTERFACE(CoapRequestHandler, GG_CoapRequestHandler) {
        CoapEndpoint_OnRequest};

static jobject CoapEndpoint_AddResourceHandlerResult_Object_From_Values(
        JNIEnv *env,
        GG_Result result,
        RequestHandler *add_resource_args
) {
    jclass add_resource_handler_result_class = env->FindClass(
            COAP_ADD_RESOURCE_HANDLER_RESULT_CLASS_NAME);
    GG_ASSERT(add_resource_handler_result_class);

    jmethodID add_resource_handler_result_constructor_id = env->GetMethodID(
            add_resource_handler_result_class,
            CONSTRUCTOR_NAME,
            COAP_ADD_RESOURCE_HANDLER_RESULT_CONSTRUCTOR_SIG);
    GG_ASSERT(add_resource_handler_result_constructor_id);

    jint result_code = (jint) result;
    jlong handler_native_reference = (jlong) (intptr_t) add_resource_args;

    jobject add_resource_handler_result_object = env->NewObject(
            add_resource_handler_result_class,
            add_resource_handler_result_constructor_id,
            result_code,
            handler_native_reference);
    GG_ASSERT(add_resource_handler_result_object);

    env->DeleteLocalRef(add_resource_handler_result_class);

    return add_resource_handler_result_object;
}

/**
 * Add a resource handler
 */
JNIEXPORT jobject
JNICALL
Java_com_fitbit_goldengate_bindings_coap_CoapEndpoint_addResourceHandler(
        JNIEnv *env,
        jobject thiz,
        jlong _endpoint,
        jstring _path,
        jobject _handler,
        jbyte _group
) {
    GG_ASSERT(_endpoint);
    GG_ASSERT(_path);
    GG_ASSERT(_handler);
    GG_ASSERT(_group >= 0 &&
              _group <= GG_COAP_GROUP_REQUEST_FILTER_MAX_GROUP);

    GG_CoapEndpoint *endpoint = (GG_CoapEndpoint *) (intptr_t) _endpoint;
    GG_ASSERT(endpoint);

    RequestHandler *add_resource_args = (RequestHandler *) GG_AllocateZeroMemory(
            sizeof(RequestHandler));
    if (add_resource_args == NULL) {
        return CoapEndpoint_AddResourceHandlerResult_Object_From_Values(
                env,
                GG_ERROR_OUT_OF_MEMORY,
                NULL);
    }

    const char *path = env->GetStringUTFChars(_path, NULL);
    GG_ASSERT(path);

    add_resource_args->endpoint = endpoint;
    add_resource_args->path = path;
    add_resource_args->response_handler = env->NewGlobalRef(_handler);
    add_resource_args->request_filter_group = _group;

    GG_SET_INTERFACE(add_resource_args, CoapRequestHandler, GG_CoapRequestHandler);


    // only for block1 handler?
    GG_CoapBlockwiseServerHelper_Init(&add_resource_args->block1_helper, GG_COAP_MESSAGE_OPTION_BLOCK1, 0);


    GG_Result result;
    Loop_InvokeSync(CoapEndpoint_AddResourceHandler, add_resource_args, &result);

    env->ReleaseStringUTFChars(_path, path);
    if (GG_FAILED(result)) {
        CoapEndpoint_ResourceHandler_Cleanup(env, add_resource_args);
    }

    return CoapEndpoint_AddResourceHandlerResult_Object_From_Values(
            env,
            result,
            add_resource_args);
}

/**
 * Remove previously added resource
 */
JNIEXPORT jint
JNICALL
Java_com_fitbit_goldengate_bindings_coap_CoapEndpoint_removeResourceHandler(
        JNIEnv *env,
        jobject thiz,
        jlong _handler
) {
    GG_ASSERT(_handler);

    RequestHandler *remove_resource_args = (RequestHandler *) (intptr_t) _handler;
    GG_ASSERT(remove_resource_args);

    GG_Result result;
    Loop_InvokeSync(CoapEndpoint_RemoveResourceHandler, remove_resource_args, &result);
    if (GG_SUCCEEDED(result)) {
        CoapEndpoint_ResourceHandler_Cleanup(env, remove_resource_args);
    }

    return result;
}

}

// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

#include "jni_gg_coap_common.h"
#include "jni_gg_coap_client_common.h"
#include <jni_gg_loop.h>
#include <string.h>
#include <logging/jni_gg_logging.h>
#include <util/jni_gg_utils.h>
#include <xp/loop/gg_loop.h>
#include <xp/coap/gg_coap.h>
#include <xp/common/gg_memory.h>

extern "C" {

/**
 * struct that implements GG_CoapResponseListener which is invoked when a response
 * for single message is received
 */
typedef struct {
    GG_IMPLEMENTS(GG_CoapResponseListener);

    // Reference to native CoAP endpoint, used to send and receive messages
    GG_CoapEndpoint *endpoint;

    // Instance of outgoing request object
    jobject outgoing_request;
    // Callback reference on which result for the request is returned
    jobject listener;

    // Parameter will contain handle to coap request, that may be used subsequently to cancel the request.
    GG_CoapRequestHandle request_handle;
} SingleCoapResponseListener;

/**
 * Cancel existing/ongoing CoAP request. Must be called from GetLoop thread
 */
static GG_Result CoapEndpoint_CancelResponseFor(void *_args) {
    SingleCoapResponseListener *args = (SingleCoapResponseListener *) _args;

    return GG_CoapEndpoint_CancelRequest(
            args->endpoint,
            args->request_handle);
}

/**
 * Send a request to coap server. Must be called from GetLoop thread
 *
 * @param _args object on which this method is invoked.
 */
static GG_Result CoapEndpoint_ResponseFor(void *_args) {
    SingleCoapResponseListener *args = (SingleCoapResponseListener *) _args;

    JNIEnv *env = Loop_GetJNIEnv();

    // get method
    GG_CoapMethod method = CoapEndpoint_GG_CoapMethod_From_Request_Object(
            env,
            args->outgoing_request);

    // get options params
    unsigned int options_count = CoapEndpoint_OptionSize_From_Message_Object(
            env,
            args->outgoing_request);
    GG_CoapMessageOptionParam options[options_count];
    CoapEndpoint_GG_CoapMessageOptionParam_From_Message_Object(
            env,
            args->outgoing_request,
            options,
            options_count);

    // get payload
    // TODO: FC-1303 - Read data from stream/ByteArray (Currently this assumes body is ByteArray)
    jbyteArray body_byte_array = CoapEndpoint_Body_ByteArray_From_OutgoingMessage_Object(
            args->outgoing_request);
    jbyte *payload = env->GetByteArrayElements(body_byte_array, NULL);
    GG_ASSERT(body_byte_array);
    int payload_size = env->GetArrayLength(body_byte_array);

    // get CoAP client parameters
    GG_CoapClientParameters coap_client_parameters = {0};
    coap_client_parameters.max_resend_count =
            (size_t)CoapEndpoint_GG_CoapMaxResendCount_From_Request_Object(
                    env,
                    args->outgoing_request);
    coap_client_parameters.ack_timeout =
            (size_t)CoapEndpoint_GG_CoapAckTimeout_From_Request_Object(
                    env,
                    args->outgoing_request);

    GG_Result result = GG_CoapEndpoint_SendRequest(
            args->endpoint,
            method,
            options,
            options_count,
            (const uint8_t *) payload,
            (unsigned int) payload_size,
            &coap_client_parameters,
            GG_CAST(args, GG_CoapResponseListener),
            &args->request_handle);

    env->ReleaseByteArrayElements(body_byte_array, payload, JNI_ABORT);
    env->DeleteLocalRef(body_byte_array);
    CoapEndpoint_ReleaseOptionParam(options, options_count);

    return result;
}

/**
 * Helper to free response object
 *
 * @param self object on which this method is invoked.
 */
static void CoapEndpoint_OnResponseCompleteCleanup(
        JNIEnv *env,
        SingleCoapResponseListener *self
) {
    if (self) {
        if (self->outgoing_request) {
            env->DeleteGlobalRef(self->outgoing_request);
        }
        if (self->listener) {
            env->DeleteGlobalRef(self->listener);
        }
        GG_ClearAndFreeObject(self, 1);
    }
}

/**
 * Method called when an ACK is received
 *
 * @param _self object on which this method is invoked.
 */
static void CoapEndpoint_OnAck(GG_CoapResponseListener *_self) {
    SingleCoapResponseListener *self = GG_SELF(
            SingleCoapResponseListener,
            GG_CoapResponseListener);
    GG_ASSERT(self->listener);

    CoapEndpoint_OnAck_Caller(self->listener);
}

/**
 * Method called when an error has occurred.
 *
 * @param _self object on which this method is invoked.
 * @param error Error code
 * @param message Error message text (may be NULL).
 */
static void CoapEndpoint_OnError(
        GG_CoapResponseListener *_self,
        GG_Result error,
        const char *message
) {
    SingleCoapResponseListener *self = GG_SELF(
            SingleCoapResponseListener,
            GG_CoapResponseListener);
    GG_ASSERT(self->listener);

    JNIEnv *env = Loop_GetJNIEnv();

    // call onError on listener callback
    CoapEndpoint_OnError_Caller(
            env,
            self->listener,
            error,
            message);

    CoapEndpoint_OnResponseCompleteCleanup(env, self);
}

/**
 * Method called when a response is received for Single request.
 *
 * @param _self object on which this method is invoked.
 * @param response The response that has been received.
 */
static void CoapEndpoint_OnResponse(GG_CoapResponseListener *_self, GG_CoapMessage *response) {
    SingleCoapResponseListener *self = GG_SELF(
            SingleCoapResponseListener,
            GG_CoapResponseListener);
    GG_ASSERT(self->listener);

    // invoke callback listener with single response message
    CoapEndpoint_OnNext_Caller(self->listener, response);

    JNIEnv *env = Loop_GetJNIEnv();
    CoapEndpoint_OnResponseCompleteCleanup(env, self);
}

// Single response Implementation for GG_CoapResponseListener interface
GG_IMPLEMENT_INTERFACE(CoapEndpoint, GG_CoapResponseListener) {
        CoapEndpoint_OnAck,
        CoapEndpoint_OnError,
        CoapEndpoint_OnResponse};

/**
 * Send a request to coap server.
 *
 * @param _endpoint reference to native stack service object to clean
 * @param _request coap request object
 * @param _listener response listener on which result will be delivered
 */
JNIEXPORT jobject
JNICALL
Java_com_fitbit_goldengate_bindings_coap_CoapEndpoint_responseFor(
        JNIEnv *env,
        jobject thiz,
        jlong _endpoint,
        jobject _request,
        jobject _listener
) {
    GG_CoapEndpoint *endpoint = (GG_CoapEndpoint *) (intptr_t) _endpoint;
    GG_ASSERT(endpoint);
    GG_ASSERT(_request);
    GG_ASSERT(_listener);

    SingleCoapResponseListener *request_for_args = (SingleCoapResponseListener *) GG_AllocateZeroMemory(
            sizeof(SingleCoapResponseListener));
    if (request_for_args == NULL) {
        CoapEndpoint_OnError_Caller(
                env,
                _listener,
                GG_ERROR_OUT_OF_MEMORY,
                "Failed to initialize memory");
        return CoapEndpoint_ResponseForResult_Object_From_Values(env, GG_ERROR_OUT_OF_MEMORY, 0);
    }

    request_for_args->endpoint = endpoint;
    request_for_args->outgoing_request = env->NewGlobalRef(_request);
    request_for_args->listener = env->NewGlobalRef(_listener);
    request_for_args->request_handle = 0;

    GG_SET_INTERFACE(request_for_args, CoapEndpoint, GG_CoapResponseListener);

    GG_Result result;
    Loop_InvokeSync(CoapEndpoint_ResponseFor, request_for_args, &result);
    if (GG_FAILED(result)) {
        CoapEndpoint_OnError_Caller(
                env,
                _listener,
                result,
                "Failed to invoke responseFor handler");
        CoapEndpoint_OnResponseCompleteCleanup(env, request_for_args);

        return CoapEndpoint_ResponseForResult_Object_From_Values(env, result, 0);
    }

    return CoapEndpoint_ResponseForResult_Object_From_Values(
            env,
            result,
            request_for_args);
}

/**
 * Cancel any pending Coap request
 *
 * @param _response_listener object holding reference to [CoapResponseListener] creating from responseFor call
 */
JNIEXPORT jint
JNICALL
Java_com_fitbit_goldengate_bindings_coap_CoapEndpoint_cancelResponseFor(
        JNIEnv *env,
        jobject thiz,
        jlong _response_listener
) {
    SingleCoapResponseListener *response_listener = (SingleCoapResponseListener *) (intptr_t) _response_listener;
    GG_ASSERT(response_listener);

    GG_Result result;
    Loop_InvokeSync(CoapEndpoint_CancelResponseFor, response_listener, &result);
    if (GG_SUCCEEDED(result)) {
        CoapEndpoint_OnResponseCompleteCleanup(env, response_listener);
    }
    return result;
}

}

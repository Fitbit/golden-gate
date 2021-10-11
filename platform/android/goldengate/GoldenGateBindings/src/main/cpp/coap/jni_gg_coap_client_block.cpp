// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

#include "jni_gg_coap_client_common.h"
#include "jni_gg_coap_common.h"
#include <jni_gg_loop.h>
#include <string.h>
#include <logging/jni_gg_logging.h>
#include <util/jni_gg_native_reference.h>
#include <util/jni_gg_utils.h>
#include <xp/loop/gg_loop.h>
#include <xp/coap/gg_coap.h>
#include <xp/coap/gg_coap_blockwise.h>
#include <xp/common/gg_memory.h>
#include <xp/coap/gg_coap_message.h>

extern "C" {

// class names
#define COAP_REQUEST_BLOCK_DATA_SOURCE_CREATOR_CLASSNAME "com/fitbit/goldengate/bindings/coap/block/CoapRequestBlockDataSourceCreator"
#define BLOCK_DATA_SOURCE_CLASS_NAME "com/fitbit/goldengate/bindings/coap/block/BlockDataSource"
#define BLOCK_SIZE_CLASS_NAME "com/fitbit/goldengate/bindings/coap/block/BlockDataSource$BlockSize"

// method names
#define BLOCK_DATA_SOURCE_GET_DATA_SIZE_NAME "getDataSize"
#define BLOCK_DATA_SOURCE_GET_DATA_NAME "getData"
#define BLOCK_SIZE_GET_SIZE_NAME "getSize"
#define BLOCK_SIZE_GET_MORE_NAME "getMore"
#define BLOCK_SIZE_GET_REQUEST_IN_RANGE_NAME "getRequestInRange"
#define BLOCK_DATA_SOURCE_CREATOR_CREATE_NAME "create"

// method signature
#define BLOCK_DATA_SOURCE_GET_DATA_SIZE_SIG "(II)L" BLOCK_SIZE_CLASS_NAME ";"
#define BLOCK_DATA_SOURCE_GET_DATA_SIG "(II)[B"
#define BLOCK_SIZE_GET_SIZE_SIG "()I"
#define BLOCK_SIZE_GET_MORE_SIG "()Z"
#define BLOCK_SIZE_GET_REQUEST_IN_RANGE_SIG "()Z"
#define BLOCK_DATA_SOURCE_CREATOR_CREATE_SIG \
    "(L" COAP_OUTGOING_REQUEST_CLASS_NAME ";)L" BLOCK_DATA_SOURCE_CLASS_NAME ";"

/**
 * struct that implements GG_CoapBlockwiseResponseListener which is invoked when a response
 * for blockwise message is received
 */
typedef struct {
    GG_IMPLEMENTS(GG_CoapBlockwiseResponseListener);
    GG_IMPLEMENTS(GG_CoapBlockSource);

    // Reference to native CoAP endpoint, used to send and receive messages
    GG_CoapEndpoint *endpoint;

    // Instance of outgoing request object
    jobject request;
    // Callback reference on which result for the request is returned
    jobject listener;
    // Local reference to determine if we have received first block or not
    jboolean started_streaming;

    // Contains reference to [BlockDataSource] if outgoing request has payload, else NULL
    jobject block_source;

    // Parameter will contain handle to coap request, that may be used subsequently to cancel the request.
    GG_CoapRequestHandle request_handle;
} ResponseListenerBlockwise;

typedef struct {
    ResponseListenerBlockwise *responseListener;
    jboolean canceled;
} CancelResponseForBlockwiseArgs;

/**
 * Send a blockwise request to coap server. Must be called from GetLoop thread
 *
 * @param _args object on which this method is invoked.
 * @thread GG Loop
 */
static GG_Result CoapEndpoint_ResponseFor_Blockwise(void *_args) {
    ResponseListenerBlockwise *args = (ResponseListenerBlockwise *) _args;

    JNIEnv *env = Loop_GetJNIEnv();

    // get method
    GG_CoapMethod method = CoapEndpoint_GG_CoapMethod_From_Request_Object(env, args->request);

    // get options params
    unsigned int options_count = CoapEndpoint_OptionSize_From_Message_Object(env, args->request);
    GG_CoapMessageOptionParam options[options_count];
    CoapEndpoint_GG_CoapMessageOptionParam_From_Message_Object(
            env,
            args->request,
            options,
            options_count);

    /**
     * payload_source is generally NULL for GET method or when user does not provide it. This check
     * here is to ensure that we pass NULL to Coap send(which is a requirement for that call) with
     * assumption that upstream always send NULL payload for GET call (see [CoapRequestBlockDataSourceCreator])
     */
    GG_CoapBlockSource *payload_source = NULL;
    if (args->block_source) {
        payload_source = GG_CAST(args, GG_CoapBlockSource);
    }

    // get CoAP client parameters
    GG_CoapClientParameters coap_client_parameters = {0};
    coap_client_parameters.max_resend_count =
            (size_t)CoapEndpoint_GG_CoapMaxResendCount_From_Request_Object(
                    env,
                    args->request);
    coap_client_parameters.ack_timeout =
            (size_t)CoapEndpoint_GG_CoapAckTimeout_From_Request_Object(
                    env,
                    args->request);

    GG_Result result = GG_CoapEndpoint_SendBlockwiseRequest(
            args->endpoint,
            method,
            options,
            options_count,
            payload_source,
            0,
            &coap_client_parameters,
            GG_CAST(args, GG_CoapBlockwiseResponseListener),
            &args->request_handle);

    CoapEndpoint_ReleaseOptionParam(options, options_count);

    return result;
}

/**
 * Helper to free response object
 *
 * @param self object on which this method is invoked.
 * @thread GG Loop
 */
static void CoapEndpoint_FreeResponseObject(
        ResponseListenerBlockwise *self
) {
    if (self) {
        JNIEnv *env = Loop_GetJNIEnv();
        if (self->request) {
            env->DeleteGlobalRef(self->request);
        }
        if (self->listener) {
            env->DeleteGlobalRef(self->listener);
        }
        if (self->block_source) {
            env->DeleteGlobalRef(self->block_source);
        }
        GG_ClearAndFreeObject(self, 2);
    }
}

/**
 * Helper target for Loop_InvokeSync that runs on the GG Loop thread.
 *
 * @param _args the ResponseListenerBlockwise struct to clean up
 * @return GG_SUCCESS always
 * @thread GG Loop
 */
static GG_Result CoapEndpoint_Cleanup_Wrapper(void *_args) {
    ResponseListenerBlockwise *args = (ResponseListenerBlockwise *) _args;
    CoapEndpoint_FreeResponseObject(args);
    return GG_SUCCESS;
}

/**
 * Cancel existing/ongoing CoAP request. Must be called from GetLoop thread
 * @thread GG Loop
 */
static GG_Result CoapEndpoint_CancelResponseFor_Blockwise(void *_args) {
    CancelResponseForBlockwiseArgs *args = (CancelResponseForBlockwiseArgs *) _args;

    if (!args->canceled){
        // ----------------------------------------------
        // args->responseListener may have been freed. We can check its fields for null, but if that memory has been reused
        // who knows what might happen.
        //--------------------------------------------------
        if (args->responseListener->endpoint != NULL &&
            args->responseListener->request_handle != GG_COAP_INVALID_REQUEST_HANDLE) {
            GG_Result result = GG_CoapEndpoint_CancelBlockwiseRequest(
                    args->responseListener->endpoint,
                    args->responseListener->request_handle);

            CoapEndpoint_FreeResponseObject(args->responseListener);
            return result;
        }
        return GG_ERROR_INVALID_STATE;
    } else {
        CoapEndpoint_FreeResponseObject(args->responseListener);
        return GG_SUCCESS;
    }
}

/**
 * Method called when a response is received for blockwise request.
 *
 * @param _self object on which this method is invoked.
 * @param block_info Details about a block of data.
 * @param block_message CoAP message
 * @thread GG Loop
 */
static void CoapEndpoint_OnResponse_Blockwise(
        GG_CoapBlockwiseResponseListener *_self,
        GG_CoapMessageBlockInfo *block_info,
        GG_CoapMessage *block_message) {
    ResponseListenerBlockwise *self = GG_SELF(
            ResponseListenerBlockwise,
            GG_CoapBlockwiseResponseListener);
    GG_ASSERT(self->listener);

    JNIEnv *env = Loop_GetJNIEnv();

    if (self->started_streaming == JNI_FALSE) {
        self->started_streaming = JNI_TRUE;

        if (block_info->offset != 0) {
            // was waiting for start, but did not receive first block
            CoapEndpoint_OnError_Caller(
                    env,
                    self->listener,
                    GG_ERROR_INTERNAL,
                    "Message start block out of order");
            return;
        }
    }

    // invoke callback listener with single response message
    CoapEndpoint_OnNext_Caller(self->listener, block_message);

    if (!block_info->more) {
        // call onComplete
        CoapEndpoint_OnComplete_Caller(self->listener);
    }
}

/**
 * Callback invoked when there is a error in requesting a coap resource
 *
 * @param _self object on which this method is invoked.
 * @param error Error code
 * @param message Error message text (may be NULL).
 * @thread GG Loop
 */
static void CoapEndpoint_OnError_Blockwise(
        GG_CoapBlockwiseResponseListener *_self,
        GG_Result error,
        const char *message) {
    ResponseListenerBlockwise *self = GG_SELF(
            ResponseListenerBlockwise,
            GG_CoapBlockwiseResponseListener);
    GG_ASSERT(self->listener);

    JNIEnv *env = Loop_GetJNIEnv();

    // call onError on listener callback
    CoapEndpoint_OnError_Caller(
            env,
            self->listener,
            error,
            message);
}

/**
 * Helper to get value of [BlockSize] from given instance of [BlockDataSource]
 * @thread GG Loop
 */
static jobject CoapEndpoint_BlockSize_Object_From_BlockSource_Object(
        JNIEnv *env,
        jobject block_source,
        size_t offset,
        size_t *data_size
) {
    jclass block_source_class = env->FindClass(BLOCK_DATA_SOURCE_CLASS_NAME);
    GG_ASSERT(block_source_class);

    jmethodID block_source_get_data_size_id = env->GetMethodID(
            block_source_class,
            BLOCK_DATA_SOURCE_GET_DATA_SIZE_NAME,
            BLOCK_DATA_SOURCE_GET_DATA_SIZE_SIG);
    GG_ASSERT(block_source_get_data_size_id);

    jobject block_size_object = env->CallObjectMethod(
            block_source,
            block_source_get_data_size_id,
            offset,
            *data_size);
    GG_ASSERT(block_size_object);

    env->DeleteLocalRef(block_source_class);

    return block_size_object;
}

/**
 * Helper to get value of [BlockSize.size]
 *
 * @thread GG Loop
 */
static int CoapEndpoint_DataSize_From_BlockSource_Object(
        JNIEnv *env,
        jclass block_size_class,
        jobject block_size_object
) {
    jmethodID block_size_get_size_id = env->GetMethodID(
            block_size_class,
            BLOCK_SIZE_GET_SIZE_NAME,
            BLOCK_SIZE_GET_SIZE_SIG);
    GG_ASSERT(block_size_get_size_id);

    return env->CallIntMethod(
            block_size_object,
            block_size_get_size_id);
}

/**
 * Helper to get value of [BlockSize.more]
 *
 * @thread GG Loop
 */
static bool CoapEndpoint_HasMoreData_From_BlockSource_Object(
        JNIEnv *env,
        jclass block_size_class,
        jobject block_size_object
) {
    jmethodID block_size_get_more_id = env->GetMethodID(
            block_size_class,
            BLOCK_SIZE_GET_MORE_NAME,
            BLOCK_SIZE_GET_MORE_SIG);
    GG_ASSERT(block_size_get_more_id);

    return env->CallBooleanMethod(
            block_size_object,
            block_size_get_more_id);
}

/**
 * Helper to get value of [BlockSize.requestInRange]
 *
 * @thread GG Loop
 */
static bool CoapEndpoint_RequestInRange_From_BlockSource_Object(
        JNIEnv *env,
        jclass block_size_class,
        jobject block_size_object
) {
    jmethodID block_size_get_request_in_range_id = env->GetMethodID(
            block_size_class,
            BLOCK_SIZE_GET_REQUEST_IN_RANGE_NAME,
            BLOCK_SIZE_GET_REQUEST_IN_RANGE_SIG);
    GG_ASSERT(block_size_get_request_in_range_id);

    return env->CallBooleanMethod(
            block_size_object,
            block_size_get_request_in_range_id);
}

/**
 * Get the data size for a given block.
 *
 * @param _self object on which this method is invoked.
 * @param offset The offset from which to read the data.
 * @param data_size [in,out] data_size The number of bytes to read from that data.
 * @param more [out] more Whether there's more data to read.
 * @return GG_SUCCESS if the requested block was not outside of the range.
 * @thread GG Loop
 */
static GG_Result CoapEndpoint_GetDataSize_Blockwise(
        GG_CoapBlockSource *_self,
        size_t offset,
        size_t *data_size,
        bool *more) {
    ResponseListenerBlockwise *self = GG_SELF(ResponseListenerBlockwise, GG_CoapBlockSource);
    GG_ASSERT(self->block_source);

    JNIEnv *env = Loop_GetJNIEnv();
    jclass block_size_class = env->FindClass(BLOCK_SIZE_CLASS_NAME);
    GG_ASSERT(block_size_class);

    jobject block_size_object = CoapEndpoint_BlockSize_Object_From_BlockSource_Object(
            env,
            self->block_source,
            offset,
            data_size);
    GG_ASSERT(block_size_object);

    bool request_in_range = CoapEndpoint_RequestInRange_From_BlockSource_Object(
            env,
            block_size_class,
            block_size_object);
    if (request_in_range) {
        *data_size = (size_t) CoapEndpoint_DataSize_From_BlockSource_Object(
                env,
                block_size_class,
                block_size_object);
        *more = CoapEndpoint_HasMoreData_From_BlockSource_Object(
                env,
                block_size_class,
                block_size_object);
    }

    env->DeleteLocalRef(block_size_class);
    env->DeleteLocalRef(block_size_object);

    return request_in_range ? GG_SUCCESS : GG_ERROR_OUT_OF_RANGE;
}

/**
 * Helper to get requested block data from [BlockSource]
 *
 * @thread GG Loop
 */
static jbyteArray CoapEndpoint_GetBlockBytes_From_BlockSource_Object(
        JNIEnv *env,
        jobject block_source,
        size_t offset,
        size_t data_size
) {
    jclass block_source_class = env->FindClass(BLOCK_DATA_SOURCE_CLASS_NAME);
    GG_ASSERT(block_source_class);
    jmethodID block_source_get_data_id = env->GetMethodID(
            block_source_class,
            BLOCK_DATA_SOURCE_GET_DATA_NAME,
            BLOCK_DATA_SOURCE_GET_DATA_SIG);
    GG_ASSERT(block_source_get_data_id);

    jobject block_data_object = env->CallObjectMethod(
            block_source,
            block_source_get_data_id,
            offset,
            data_size);
    GG_ASSERT(block_data_object);

    env->DeleteLocalRef(block_source_class);

    return (jbyteArray) block_data_object;
}

/**
 * Get the data for a given block.
 *
 * @param _self The object on which this method is invoked.
 * @param offset The offset of the requested block in bytes.
 * @param data_size The size of the requested block in bytes.
 * @param data Pointer to the buffer in which the block data should be copied.
 * @return GG_SUCCESS if the data for the requested block could be copied, or a negative error code.
 * @thread GG Loop
 */
static GG_Result CoapEndpoint_GetData_Blockwise(
        GG_CoapBlockSource *_self,
        size_t offset,
        size_t data_size,
        void *data) {
    ResponseListenerBlockwise *self = GG_SELF(ResponseListenerBlockwise, GG_CoapBlockSource);
    GG_ASSERT(self->block_source);

    JNIEnv *env = Loop_GetJNIEnv();
    jbyteArray block_data_object = CoapEndpoint_GetBlockBytes_From_BlockSource_Object(
            env,
            self->block_source,
            offset,
            data_size);
    jbyte *block_byte_array = env->GetByteArrayElements(block_data_object, NULL);
    GG_ASSERT(block_byte_array);
    int block_byte_array_size = env->GetArrayLength(block_data_object);

    // return requested block data
    data = memcpy(data, block_byte_array, (size_t) block_byte_array_size);

    env->ReleaseByteArrayElements(block_data_object, block_byte_array, JNI_ABORT);
    env->DeleteLocalRef(block_data_object);

    return GG_SUCCESS;
}

/**
 * Creates a new global [BlockDataSource] object if a request has body
 *
 * @param self instance of ResponseListenerBlockwise that will get its data source object created
 * @thread any
 */
static void CoapEndpoint_BlockSource_From_RequestListener(
        JNIEnv *env,
        ResponseListenerBlockwise *self
) {
    GG_ASSERT(self->request);

    jclass block_source_creator_class = env->FindClass(COAP_REQUEST_BLOCK_DATA_SOURCE_CREATOR_CLASSNAME);
    GG_ASSERT(block_source_creator_class);
    jclass block_source_class = env->FindClass(BLOCK_DATA_SOURCE_CLASS_NAME);
    GG_ASSERT(block_source_class);

    jmethodID block_source_creator_create_id = env->GetMethodID(
            block_source_creator_class,
            BLOCK_DATA_SOURCE_CREATOR_CREATE_NAME,
            BLOCK_DATA_SOURCE_CREATOR_CREATE_SIG);
    GG_ASSERT(block_source_creator_create_id);

    jmethodID block_source_creator_constructor = env->GetMethodID(
            block_source_creator_class,
            CONSTRUCTOR_NAME,
            DEFAULT_CONSTRUCTOR_SIG);
    GG_ASSERT(block_source_creator_constructor);

    jobject block_source_creator_object = env->NewObject(
            block_source_creator_class,
            block_source_creator_constructor);
    GG_ASSERT(block_source_creator_object);

    jobject block_source_object = env->CallObjectMethod(
            block_source_creator_object,
            block_source_creator_create_id,
            self->request);

    if (block_source_object) {
        self->block_source = env->NewGlobalRef(block_source_object);
    }

    env->DeleteLocalRef(block_source_creator_class);
    env->DeleteLocalRef(block_source_class);
    env->DeleteLocalRef(block_source_creator_object);
    env->DeleteLocalRef(block_source_object);
}

// Blockwise response Implementation for GG_CoapBlockwiseResponseListener interface
GG_IMPLEMENT_INTERFACE(BlockListener, GG_CoapBlockwiseResponseListener) {
        .OnResponseBlock = CoapEndpoint_OnResponse_Blockwise,
        .OnError         = CoapEndpoint_OnError_Blockwise};

// Blockwise response Implementation for GG_CoapBlockSource interface
GG_IMPLEMENT_INTERFACE(BlockListener, GG_CoapBlockSource) {
        .GetDataSize = CoapEndpoint_GetDataSize_Blockwise,
        .GetData = CoapEndpoint_GetData_Blockwise};

/**
 * Send a blockwise request to coap server.
 *
 * @param _endpoint reference to native stack service object
 * @param _request coap request object
 * @param _listener response listener on which result will be delivered
 * @thread any
 */
JNIEXPORT jobject
JNICALL
Java_com_fitbit_goldengate_bindings_coap_CoapEndpoint_responseForBlockwise(
        JNIEnv *env,
        jobject thiz,
        jlong _endpoint_wrapper,
        jobject _request,
        jobject _listener
) {

    GG_ASSERT(_endpoint_wrapper);
    GG_ASSERT(_request);
    GG_ASSERT(_listener);

    NativeReferenceWrapper *endpoint_wrapper = (NativeReferenceWrapper *) (intptr_t) _endpoint_wrapper;
    GG_ASSERT(endpoint_wrapper);
    GG_CoapEndpoint *endpoint = (GG_CoapEndpoint*)endpoint_wrapper->pointer;
    GG_ASSERT(endpoint);

    ResponseListenerBlockwise *request_for_args = (ResponseListenerBlockwise *) GG_AllocateZeroMemory(
            sizeof(ResponseListenerBlockwise));
    if (request_for_args == NULL) {
        CoapEndpoint_OnError_Caller(
                env,
                _listener,
                GG_ERROR_OUT_OF_MEMORY,
                "Failed to initialize memory");
        return CoapEndpoint_ResponseForResult_Object_From_Values(env, GG_ERROR_OUT_OF_MEMORY, 0);
    }

    request_for_args->endpoint = endpoint;
    request_for_args->request = env->NewGlobalRef(_request);
    request_for_args->listener = env->NewGlobalRef(_listener);
    request_for_args->started_streaming = JNI_FALSE;
    request_for_args->request_handle = 0;

    // create a data source if request has body (limited to PUT and POST calls)
    CoapEndpoint_BlockSource_From_RequestListener(env, request_for_args);

    GG_SET_INTERFACE(request_for_args, BlockListener, GG_CoapBlockwiseResponseListener);
    GG_SET_INTERFACE(request_for_args, BlockListener, GG_CoapBlockSource);

    GG_Result result;
    Loop_InvokeSync(CoapEndpoint_ResponseFor_Blockwise, request_for_args, &result);
    if (GG_FAILED(result)) {
        CoapEndpoint_OnError_Caller(
                env,
                _listener,
                result,
                "Failed to invoke responseFor handler");
        GG_Result cleanupResult;
        Loop_InvokeSync(CoapEndpoint_Cleanup_Wrapper, request_for_args, &cleanupResult);

        return CoapEndpoint_ResponseForResult_Object_From_Values(env, result, 0);
    }

  CoapEndpoint_SetNativeListenerReference(env, _listener, request_for_args);

    return CoapEndpoint_ResponseForResult_Object_From_Values(
            env,
            result,
            request_for_args);
}

/**
 * Cancel any pending Coap request and clean up [ResponseListenerBlockwise] object
 *
 * @param _response_listener object holding reference to [CoapResponseListener] creating from responseFor call
 * @param _canceled set to true if the ongoing blockwise coap request has been canceled
 * @thread any
 */
JNIEXPORT jint
JNICALL
Java_com_fitbit_goldengate_bindings_coap_block_BlockwiseCoapResponseListener_cancelResponseForBlockwise(
    JNIEnv *env,
    jobject thiz,
    jlong _response_listener,
    jboolean _canceled
) {
    ResponseListenerBlockwise *response_listener = (ResponseListenerBlockwise *) (intptr_t) _response_listener;
    GG_ASSERT(response_listener);

    CancelResponseForBlockwiseArgs args = {
        .responseListener = response_listener,
        .canceled = _canceled
    };

    GG_Result result;
    Loop_InvokeSync(CoapEndpoint_CancelResponseFor_Blockwise, &args, &result);
    return result;
}

}

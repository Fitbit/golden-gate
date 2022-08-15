// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

#include "jni_gg_coap_common.h"
#include <jni_gg_loop.h>
#include <string.h>
#include <xp/coap/gg_coap_blockwise.h>
#include <platform/android/goldengate/GoldenGateBindings/src/main/cpp/logging/jni_gg_logging.h>
#include "jni_gg_coap_server.h"

extern "C" {

// class names
#define COAP_RESPONSE_BLOCK_DATA_SOURCE_CREATOR_CLASSNAME "com/fitbit/goldengate/bindings/coap/block/CoapResponseBlockDataSourceCreator"
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
    "(L" COAP_OUTGOING_RESPONSE_CLASS_NAME ";)L" BLOCK_DATA_SOURCE_CLASS_NAME ";"

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
    RequestHandler *self = GG_SELF(RequestHandler, GG_CoapBlockSource);
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

    *data_size = (size_t) CoapEndpoint_DataSize_From_BlockSource_Object(
            env,
            block_size_class,
            block_size_object);
    *more = CoapEndpoint_HasMoreData_From_BlockSource_Object(
            env,
            block_size_class,
            block_size_object);

    env->DeleteLocalRef(block_size_class);
    env->DeleteLocalRef(block_size_object);

    return GG_SUCCESS;
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
    RequestHandler *self = GG_SELF(RequestHandler, GG_CoapBlockSource);
    GG_ASSERT(self->block_source);

    JNIEnv *env = Loop_GetJNIEnv();
    jbyteArray block_data_object = CoapEndpoint_GetBlockBytes_From_BlockSource_Object(
            env,
            self->block_source,
            offset,
            data_size);

    if (block_data_object) {
        jbyte *block_byte_array = env->GetByteArrayElements(block_data_object, NULL);
        GG_ASSERT(block_byte_array);
        int block_byte_array_size = env->GetArrayLength(block_data_object);

        // return requested block data
        data = memcpy(data, block_byte_array, (size_t) block_byte_array_size);

        env->ReleaseByteArrayElements(block_data_object, block_byte_array, JNI_ABORT);
        env->DeleteLocalRef(block_data_object);
    }

    return GG_SUCCESS;
}

/**
 * Creates a new global [BlockDataSource] object if a request has body
 *
 * @param self instance of ResponseListenerBlockwise that will get its data source object created
 * @thread any
 */
static void CoapEndpoint_BlockSource_From_Response_Object(
        JNIEnv *env,
        RequestHandler *self,
        jobject outgoing_response_object
) {

    jclass block_source_creator_class = env->FindClass(COAP_RESPONSE_BLOCK_DATA_SOURCE_CREATOR_CLASSNAME);
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
            outgoing_response_object);

    if (block_source_object) {
        self->block_source = env->NewGlobalRef(block_source_object);
    } else {
        self->block_source = nullptr;
    }

    env->DeleteLocalRef(block_source_creator_class);
    env->DeleteLocalRef(block_source_class);
    env->DeleteLocalRef(block_source_creator_object);
    env->DeleteLocalRef(block_source_object);
}

GG_IMPLEMENT_INTERFACE(BlockSource, GG_CoapBlockSource) {
        .GetDataSize = CoapEndpoint_GetDataSize_Blockwise,
        .GetData = CoapEndpoint_GetData_Blockwise};
}

/**
 * Helper method to create the block source object from response object and
 * then create blockwise response for Block2 with payload supplied by the block source
 *
 * @param endpoint The endpoint that creates the response.
 * @param request_handler The request handler structure
 * @param outgoing_response_object The response object which is returned from request handler
 * @param request The request for which the response is created.
 * @param block_info Details about the block.
 * @param response Pointer to the variable in which the object will be returned.
 * @return GG_SUCCESS if the object could be created, or a negative error code.
 */
GG_Result CoapEndpoint_CreateBlockwiseResponseFromBlockSource(
        JNIEnv *env,
        GG_CoapEndpoint *endpoint,
        RequestHandler *request_handler,
        jobject outgoing_response_object,
        const GG_CoapMessage *request,
        GG_CoapMessageBlockInfo *block_info,
        GG_CoapMessage **response
) {
    // create a data source if the response has body
    CoapEndpoint_BlockSource_From_Response_Object(env, request_handler, outgoing_response_object);
    GG_SET_INTERFACE(request_handler, BlockSource, GG_CoapBlockSource);

    GG_CoapBlockSource *payload_source = NULL;

    if (request_handler->block_source) {
        payload_source = GG_CAST(request_handler, GG_CoapBlockSource);
    }

    return GG_CoapEndpoint_CreateBlockwiseResponseFromBlockSource(
            endpoint,
            request,
            GG_COAP_MESSAGE_CODE_CONTENT,
            NULL,
            0,
            payload_source,
            GG_COAP_MESSAGE_OPTION_BLOCK2,
            block_info,
            response);
}

/**
 * Helper method to use GG_CoapBlockwiseServerHelper object to support blockwise transfers and
 * create blockwise response for Block2
 *
 * @param endpoint The endpoint that creates the response.
 * @param request_handler The request handler structure
 * @param request The request for which the response is created.
 * @param response Pointer to the variable in which the object will be returned.
 * @return GG_SUCCESS if the object could be created, or a negative error code.
 */
GG_Result CoapEndpoint_CreateBlockwiseResponseWithServerHelper(
        GG_CoapEndpoint *endpoint,
        RequestHandler *request_handler,
        const GG_CoapMessage *request,
        GG_CoapMessage **response
) {

    bool request_was_resent = false;
    GG_Result result = GG_CoapBlockwiseServerHelper_OnRequest(&request_handler->block1_helper,
                                                              request,
                                                              &request_was_resent);

    if (result != GG_SUCCESS) {
        return result;
    }

    return GG_CoapBlockwiseServerHelper_CreateResponse(
            &request_handler->block1_helper,
            endpoint,
            request,
            request_handler->block1_helper.block_info.more ?
            GG_COAP_MESSAGE_CODE_CONTINUE :
            GG_COAP_MESSAGE_CODE_CHANGED,
            NULL,
            0,
            NULL,
            0,
            response);
}

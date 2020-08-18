// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

#include <jni.h>
#include <xp/coap/gg_coap.h>

#ifndef GOLDENGATELIB_JNI_GG_COAP_SERVER_BLOCK_H
#define GOLDENGATELIB_JNI_GG_COAP_SERVER_BLOCK_H

/**
 * Helper method to create the block source object from response object and
 * then create blockwise response with payload supplied by the block source
 *
 * @param endpoint The endpoint that creates the response.
 * @param request_handler The request handler structure
 * @param outgoing_response_object The response object which is returned from request handler
 * @param request The request for which the response is created.
 * @param response Pointer to the variable in which the object will be returned.
 * @return GG_SUCCESS if the object could be created, or a negative error code.
 */
GG_Result CoapEndpoint_CreateBlockwiseResponseFromBlockSource(
        JNIEnv *env,
        GG_CoapEndpoint *endpoint,
        RequestHandler *request_handler,
        jobject outgoing_response_object,
        const GG_CoapMessage *request,
        GG_CoapMessage **response
);

#endif //GOLDENGATELIB_JNI_GG_COAP_SERVER_BLOCK_H

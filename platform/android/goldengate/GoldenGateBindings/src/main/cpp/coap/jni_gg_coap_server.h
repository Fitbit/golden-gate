// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

#include <jni.h>
#include <xp/coap/gg_coap.h>

#ifndef GOLDENGATELIB_JNI_GG_COAP_SERVER_H
#define GOLDENGATELIB_JNI_GG_COAP_SERVER_H

extern "C" {

/**
* struct that implements GG_CoapRequestHandler which is invoked when a there is a new request for
* registered endpoint path
*/
typedef struct {
    GG_IMPLEMENTS(GG_CoapRequestHandler);
    GG_IMPLEMENTS(GG_CoapBlockSource);

    GG_CoapEndpoint *endpoint;
    const char *path;
    jobject response_handler;
    jbyte request_filter_group;
    jobject block_source;

    GG_CoapBlockwiseServerHelper block1_helper;

} RequestHandler;

};

#endif //GOLDENGATELIB_JNI_GG_COAP_SERVER_H


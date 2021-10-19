// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

#include "platform/android/goldengate/GoldenGateBindings/src/main/cpp/coap/jni_gg_coap_common.h"

#ifndef JNI_GG_COAP_CLIENT_COMMON_H
#define JNI_GG_COAP_CLIENT_COMMON_H

extern "C" {

/**
 * Helper method to invoke onAck method on provided listener
 *
 * @param listener reference to kotlin CoapResponseListener object
 */
void CoapEndpoint_OnAck_Caller(jobject listener);

/**
 * Helper method to invoke onError method on provided listener
 *
 * @param listener reference to kotlin CoapResponseListener object
 * @param error Error code
 * @param message Error message text (may be NULL).
 */
void CoapEndpoint_OnError_Caller(
        JNIEnv *env,
        jobject listener,
        GG_Result error,
        const char *message
);

/**
 * Helper method to invoke onNext method on provided listener
 *
 * @param listener reference to kotlin CoapResponseListener object
 * @param response single or blockwise coap response message
 */
void CoapEndpoint_OnNext_Caller(jobject listener, GG_CoapMessage *response);

/**
 * Helper method to invoke onComplete method on provided listener
 *
 * @param listener reference to kotlin CoapResponseListener object
 */
void CoapEndpoint_OnComplete_Caller(jobject listener);

/**
 * Create [ResponseForResult] kotlin object from given values
 */
jobject CoapEndpoint_ResponseForResult_Object_From_Values(
        JNIEnv *env,
        GG_Result result,
        void *response_listener
);

/**
 * Helper method to invoke setNativeReference method on provided listener
 *
 * @param listener reference to kotlin CoapResponseListener object
 * @param native response listener reference
 */
void CoapEndpoint_SetNativeListenerReference(
    JNIEnv *env,
    jobject listener,
    void *_response_listener
);

}
#endif // JNI_GG_COAP_CLIENT_COMMON_H

// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

#include "platform/android/goldengate/GoldenGateBindings/src/main/cpp/jni_gg_loop.h"
#include "platform/android/goldengate/GoldenGateBindings/src/main/cpp/coap/jni_gg_coap_common.h"
#include "platform/android/goldengate/GoldenGateBindings/src/main/cpp/logging/jni_gg_logging.h"
#include "platform/android/goldengate/GoldenGateBindings/src/main/cpp/util/jni_gg_native_reference.h"
#include "xp/common/gg_common.h"
#include "xp/services/coap_client/gg_coap_client_service.h"
/**
 * Jni Bindings for CoapGeneratorService
 */
extern "C" {

    /**
     * Create native Coap Generator service object.
     *
     * @return reference to native coap generator service
     */
    JNIEXPORT jlong
    JNICALL
    Java_com_fitbit_goldengate_bindings_services_CoapGeneratorService_create(
            JNIEnv *env,
            jobject thiz,
            jlong _endpoint_wrapper
    ) {

        NativeReferenceWrapper *endpoint_wrapper = (NativeReferenceWrapper *) (intptr_t) _endpoint_wrapper;
        if (!endpoint_wrapper || !endpoint_wrapper->pointer) {
            return 0;
        }

        GG_CoapClientService *service = NULL;

        GG_Result result = GG_CoapClientService_Create(
                Loop_GetLoop(),
                (GG_CoapEndpoint*) endpoint_wrapper->pointer,
                &service);

        if (GG_FAILED(result)) {
            GG_Log_JNI("CoapGeneratorService", "GG_CoapClientService_Create failed with error code %d", result);
        }

        return (jlong)(intptr_t) service;
    }

    /**
     * Register the CoapGenerator service with a remote shell.
     *
     * @param shellPtr reference to remote shell
     * @param selfPtr reference to native CoapGenerator service object
     */
    JNIEXPORT void JNICALL
    Java_com_fitbit_goldengate_bindings_services_CoapGeneratorService_register(
            JNIEnv * env,
            jobject thiz,
            jlong selfPtr,
            jlong remoteShellPtr
    ) {
        GG_CoapClientService* self = (GG_CoapClientService*) (intptr_t) selfPtr;
        GG_RemoteShell* remoteShell = (GG_RemoteShell*) (intptr_t) remoteShellPtr;
        GG_ASSERT(self);
        GG_ASSERT(remoteShell);

        GG_Result result = GG_CoapClientService_Register(self, remoteShell);

        if (GG_FAILED(result)) {
            GG_Log_JNI("CoapGeneratorService", "GG_CoapClientService_Register failed with error code %d", result);
        }
    }


    /**
     * Destroy Coap Generator service previously created with [create] method.
     *
     * @param selfPtr reference to native Coap Generator service object to clean
     */
    JNIEXPORT void
    Java_com_fitbit_goldengate_bindings_services_CoapGeneratorService_destroy(
            JNIEnv * env,
            jobject thiz,
            jlong selfPtr
    ) {
        GG_CoapClientService* self = (GG_CoapClientService*) (intptr_t) selfPtr;
        GG_FreeMemory(self);
    }

}

// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0"
#include "platform/android/goldengate/GoldenGateBindings/src/main/cpp/jni_gg_loop.h"
#include "platform/android/goldengate/GoldenGateBindings/src/main/cpp/coap/jni_gg_coap_common.h"
#include "platform/android/goldengate/GoldenGateBindings/src/main/cpp/logging/jni_gg_logging.h"
#include "platform/android/goldengate/GoldenGateBindings/src/main/cpp/util/jni_gg_native_reference.h"
#include "xp/coap/gg_coap.h"
#include "xp/services/test_server/gg_coap_test_service.h"

extern "C" {

    typedef struct {
        GG_CoapEndpoint * coapEndpoint;
        GG_CoapTestService * coapTestService;
    } TestServiceBuildArgs;

    typedef struct {
        GG_CoapTestService * coapTestService;
        GG_RemoteShell * remoteShell;
    } TestServiceRegistrationArgs;

    GG_Result CreateTestService(void *buildStackArgs) {
        TestServiceBuildArgs * testServiceArgs = (TestServiceBuildArgs *) buildStackArgs;
        GG_ASSERT(testServiceArgs);

        return GG_CoapTestService_Create(testServiceArgs->coapEndpoint, &testServiceArgs->coapTestService);
    }

    JNIEXPORT jlong
    JNICALL
    Java_com_fitbit_goldengate_bindings_services_CoapTestService_create(
            JNIEnv *env,
            jobject thiz,
            jlong _endpoint_wrapper
            ) {
        NativeReferenceWrapper *endpoint_wrapper = (NativeReferenceWrapper *) (intptr_t) _endpoint_wrapper;
        if (!endpoint_wrapper || !endpoint_wrapper->pointer) {
            return 0;
        }

        TestServiceBuildArgs * testServiceArgs =
                (TestServiceBuildArgs *) GG_AllocateZeroMemory(sizeof(TestServiceBuildArgs));
        testServiceArgs->coapEndpoint = (GG_CoapEndpoint*)endpoint_wrapper->pointer;

        GG_Result result;
        Loop_InvokeSync(CreateTestService, testServiceArgs, &result);

        GG_CoapTestService * testService = testServiceArgs->coapTestService;

        GG_FreeMemory(testServiceArgs);

        if (GG_FAILED(result)) {
            GG_Log_JNI("CoapTestService", "GG_CoapTestService_Create failed with error code %d", result);
            return -1;
        }

        return (intptr_t) testService;
    }

    GG_Result RegisterTestService(void* args) {
        TestServiceRegistrationArgs * testServiceArgs = (TestServiceRegistrationArgs *) args;

        return GG_CoapTestService_RegisterSmoHandlers(testServiceArgs->remoteShell,
                GG_CoapTestService_AsRemoteSmoHandler(testServiceArgs->coapTestService));
    }

    JNIEXPORT void
    JNICALL
    Java_com_fitbit_goldengate_bindings_services_CoapTestService_register(
            JNIEnv *env,
            jobject thiz,
            jlong coapTestServicePtr,
            jlong remoteShellPtr
            ) {
        GG_CoapTestService * coapTestService = (GG_CoapTestService *) (intptr_t) coapTestServicePtr;
        GG_ASSERT(coapTestService);

        GG_RemoteShell* remoteShell = (GG_RemoteShell *) (intptr_t) remoteShellPtr;
        GG_ASSERT(remoteShell);

        TestServiceRegistrationArgs * args = (TestServiceRegistrationArgs *) GG_AllocateZeroMemory(
                sizeof(TestServiceRegistrationArgs));

        args->coapTestService = coapTestService;
        args->remoteShell = remoteShell;

        GG_Result result;
        Loop_InvokeSync(RegisterTestService, args, &result);

        if (GG_FAILED(result)) {
            GG_Log_JNI("CoapTestService", "GG_CoapTestService_RegisterSmoHandlers failed with error code %d", result);
        }
    }

    void DestroyTestService(void* args) {
        GG_CoapTestService * testService = (GG_CoapTestService *)args;
        GG_CoapTestService_Destroy(testService);
    }

    JNIEXPORT void
    JNICALL
    Java_com_fitbit_goldengate_bindings_services_CoapTestService_destroy(JNIEnv *env,
                                                                         jobject thiz,
                                                                         jlong coapTestServicePtr) {

        GG_CoapTestService * coapTestService = (GG_CoapTestService *) (intptr_t) coapTestServicePtr;
        GG_ASSERT(coapTestService);

        Loop_InvokeAsync(DestroyTestService, coapTestService);
    }
// End extern "C"
}

// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

#include "platform/android/goldengate/GoldenGateBindings/src/main/cpp/jni_gg_loop.h"
#include "platform/android/goldengate/GoldenGateBindings/src/main/cpp/logging/jni_gg_logging.h"
#include "xp/common/gg_common.h"
#include "xp/services/blast/gg_blast_service.h"


/**
 * Jni Bindings for BlastService
 */
extern "C" {

    /**
     * Create native Blast service object.
     *
     * @return reference to native blast service
     */
    JNIEXPORT jlong JNICALL
    Java_com_fitbit_goldengate_bindings_services_BlastService_create(
            JNIEnv * env,
            jobject thiz
    ) {
        GG_BlastService* service = NULL;

        GG_Result result = GG_BlastService_Create(Loop_GetLoop(), &service);

        if (GG_FAILED(result)) {
            GG_Log_JNI("BlastService", "GG_BlastService_Create failed with error code %d", result);
        }

        return (jlong) (intptr_t) service;
    }

    /**
     * Register the Blast service with a remote shell.
     *
     * @param shellPtr reference to remote shell
     * @param selfPtr reference to native Blast service object
     */
    JNIEXPORT void JNICALL
    Java_com_fitbit_goldengate_bindings_services_BlastService_register(
            JNIEnv * env,
            jobject thiz,
            jlong selfPtr,
            jlong shellPtr
    ) {
        GG_BlastService* self = (GG_BlastService*) (intptr_t) selfPtr;
        GG_RemoteShell* shell = (GG_RemoteShell*) (intptr_t) shellPtr;

        GG_Result result = GG_BlastService_Register(self, shell);

        if (GG_FAILED(result)) {
            GG_Log_JNI("BlastService", "GG_BlastService_Register failed with error code %d", result);
        }
    }

    /**
     * Set the source and sink for the service. Used for attaching the service to a stack.
     *
     * @param selfPtr reference to native Blast service object
     * @param sourcePtr reference to the source to use for receiving data
     * @param sinkPtr reference to the sink to use for sending data
     */
    JNIEXPORT void JNICALL
    Java_com_fitbit_goldengate_bindings_services_BlastService_attach(
            JNIEnv * env,
            jobject thiz,
            jlong selfPtr,
            jlong sourcePtr,
            jlong sinkPtr
    ) {
        GG_BlastService* self = (GG_BlastService*) (intptr_t) selfPtr;
        GG_DataSource* source = (GG_DataSource*) (intptr_t) sourcePtr;
        GG_DataSink* sink = (GG_DataSink*) (intptr_t) sinkPtr;

        GG_Result result = GG_BlastService_Attach(self, source, sink);

        if (GG_FAILED(result)) {
            GG_Log_JNI("BlastService", "GG_BlastService_Attach failed with error code %d", result);
        }
    }

    /**
     * Detach the service from the stack previously attached with [attach].
     *
     * @param selfPtr reference to native Blast service object
     */
    JNIEXPORT void JNICALL
    Java_com_fitbit_goldengate_bindings_services_BlastService_detach(
            JNIEnv * env,
            jobject thiz,
            jlong selfPtr
    ) {
        GG_BlastService* self = (GG_BlastService*) (intptr_t) selfPtr;

        GG_Result result = GG_BlastService_Attach(self, NULL, NULL);

        if (GG_FAILED(result)) {
            GG_Log_JNI("BlastService", "GG_BlastService_Attach failed with error code %d", result);
        }
    }

    /**
     * Destroy Blast service previously created with [create] method.
     *
     * @param selfPtr reference to native Blast service object to clean
     */
        JNIEXPORT void
    Java_com_fitbit_goldengate_bindings_services_BlastService_destroy(
            JNIEnv * env,
            jobject thiz,
            jlong selfPtr
    ) {
        GG_BlastService* self = (GG_BlastService*) (intptr_t) selfPtr;
        GG_BlastService_Destroy(self);
    }

}

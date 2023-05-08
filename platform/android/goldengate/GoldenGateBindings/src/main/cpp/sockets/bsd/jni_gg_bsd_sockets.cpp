// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

#include <jni.h>
#include <string.h>
#include <stdlib.h>
#include "third_party/goldengate/platform/android/goldengate/GoldenGateBindings/src/main/cpp/jni_gg_loop.h"
#include "third_party/goldengate/platform/android/goldengate/GoldenGateBindings/src/main/cpp/util/jni_gg_utils.h"
#include "third_party/goldengate/platform/android/goldengate/GoldenGateBindings/src/main/cpp/sockets/jni_gg_socket_address.h"
#include "third_party/goldengate/platform/android/goldengate/GoldenGateBindings/src/main/cpp/logging/jni_gg_logging.h"
#include "third_party/goldengate/xp/loop/gg_loop.h"
#include "third_party/goldengate/xp/sockets/gg_sockets.h"
#include "third_party/goldengate/xp/sockets/ports/bsd/gg_bsd_sockets.h"
#include "third_party/goldengate/xp/common/gg_port.h"
#include "third_party/goldengate/xp/coap/gg_coap.h"
#include "third_party/goldengate/xp/common/gg_common.h"

extern "C" {

/** Setting default to minimum datagram size*/
#define DEFAULT_MAX_DATAGRAM_SIZE 1280
/** Setting to false allows to accept connection from ip besides one in remote socket address*/
#define DEFAULT_CONNECT_TO_REMOTE false

/**
 * Struct to hold data for creating BsdDatagramSocket
 */
typedef struct {
    GG_SocketAddress *gg_local_socket_address;
    GG_SocketAddress *gg_remote_socket_address;

    GG_DatagramSocket *socket;
} BsdDatagramSocket_CreateArgs;

/**
 * Create a BsdDatagramSocket, must be called from GG_Loop thread
 */
static int BsdDatagramSocket_Create(void *_args) {
    BsdDatagramSocket_CreateArgs *args = (BsdDatagramSocket_CreateArgs *) _args;

    GG_Result result = GG_BsdDatagramSocket_Create(
            args->gg_local_socket_address,
            args->gg_remote_socket_address,
            DEFAULT_CONNECT_TO_REMOTE,
            DEFAULT_MAX_DATAGRAM_SIZE,
            &args->socket);
    if (GG_FAILED(result)) {
        return result;
    }
    return GG_DatagramSocket_Attach(args->socket, Loop_GetLoop());
}

/**
 * Destroy previously created BsdDatagramSocket, must be called from GG_Loop thread
 * @param _args
 */
static void BsdDatagramSocket_Destroy(void *_args) {
    GG_DatagramSocket *args = (GG_DatagramSocket *) _args;
    GG_DatagramSocket_Destroy(args);
}

/**
 * Jni call for creating BsdDatagramSocket
 *
 * @param remote_ip_address remote IP address for this socket to bound to, can be NULL if this
 * socket is used as service only accepting request from any client
 * @param remote_port remote port this socket to bound to, if remote_ip_address is NULL this
 * field is ignored
 * @param local_port local port this socket to bound to
 * @return native reference to newly created BsdDatagramSocket
 */
JNIEXPORT jlong JNICALL
Java_com_fitbit_goldengate_bindings_sockets_bsd_BsdDatagramSocket_create(
        JNIEnv *env,
        jobject thiz,
        jint local_port,
        jobject remote_inet_address,
        jint remote_port
) {
    GG_ASSERT(local_port);

    GG_SocketAddress *gg_local_socket_address = (GG_SocketAddress *) GG_AllocateZeroMemory(
            sizeof(GG_SocketAddress));
    gg_local_socket_address->address = GG_IpAddress_Any;
    gg_local_socket_address->port = (uint16_t) local_port;

    GG_SocketAddress *gg_remote_socket_address = NULL;
    if (remote_inet_address) {
        GG_IpAddress gg_ip_address = GG_IpAddress_From_Inet4Address_Object(env, remote_inet_address);
        gg_remote_socket_address = (GG_SocketAddress *) GG_AllocateZeroMemory(
                sizeof(GG_SocketAddress));
        gg_remote_socket_address->address = gg_ip_address;
        gg_remote_socket_address->port = (uint16_t) remote_port;
    }

    BsdDatagramSocket_CreateArgs create_args = {
            .gg_local_socket_address = gg_local_socket_address,
            .gg_remote_socket_address = gg_remote_socket_address
    };

    GG_Result result;
    Loop_InvokeSync(BsdDatagramSocket_Create, &create_args, &result);

    GG_FreeMemory(gg_local_socket_address);
    if (gg_remote_socket_address) {
        GG_FreeMemory(gg_remote_socket_address);
    }

    if (GG_FAILED(result)) {
        GG_Log_JNI("BsdDatagramSocket", "Create failed with error code %d", result);
        return result;
    }
    return (jlong) (intptr_t) create_args.socket;
}

/**
 * Jni call to destroy previously created BsdDatagramSocket
 *
 * @param socketPtr pointer to previously created BsdDatagramSocket
 */
JNIEXPORT void JNICALL
Java_com_fitbit_goldengate_bindings_sockets_bsd_BsdDatagramSocket_destroy(
        JNIEnv *env,
        jobject thiz,
        jlong socketPtr
) {
    GG_ASSERT(socketPtr);

    GG_DatagramSocket *gg_datagram_socket = (GG_DatagramSocket *) (intptr_t) socketPtr;
    Loop_InvokeAsync(BsdDatagramSocket_Destroy, gg_datagram_socket);
}

/**
 * Get reference as DataSource to previously created BsdDatagramSocket
 *
 * @param socketPtr pointer to previously created BsdDatagramSocket
 * @return native pointer to BsdDatagramSocket as DataSource
 */
JNIEXPORT jlong JNICALL
Java_com_fitbit_goldengate_bindings_sockets_bsd_BsdDatagramSocket_asDataSource(
        JNIEnv *env,
        jobject thiz,
        jlong socketPtr
) {
    GG_ASSERT(socketPtr);

    GG_DatagramSocket *gg_datagram_socket = (GG_DatagramSocket *) (intptr_t) socketPtr;
    GG_DataSource *other_endpoint_as_source = GG_DatagramSocket_AsDataSource(gg_datagram_socket);

    return (jlong) (intptr_t) other_endpoint_as_source;
}

/**
 * Get reference as DataSink to previously created BsdDatagramSocket
 *
 * @param socketPtr pointer to previously created BsdDatagramSocket
 * @return native pointer to BsdDatagramSocket as DataSink
 */
JNIEXPORT jlong JNICALL
Java_com_fitbit_goldengate_bindings_sockets_bsd_BsdDatagramSocket_asDataSink(
        JNIEnv *env,
        jobject thiz,
        jlong socketPtr
) {
    GG_ASSERT(socketPtr);

    GG_DatagramSocket *gg_datagram_socket = (GG_DatagramSocket *) (intptr_t) socketPtr;
    GG_DataSink *other_endpoint_as_sink = GG_DatagramSocket_AsDataSink(gg_datagram_socket);

    return (jlong) (intptr_t) other_endpoint_as_sink;
}

}

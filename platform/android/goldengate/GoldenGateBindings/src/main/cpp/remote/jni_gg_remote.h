// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

#include <xp/common/gg_types.h>
#include <xp/remote/gg_remote.h>
#include <jni.h>

#ifndef GOLDENGATE_JNI_GG_REMOTE_H
#define GOLDENGATE_JNI_GG_REMOTE_H

extern "C" {

typedef struct {
    GG_IMPLEMENTS(GG_RemoteCborHandler);
    jobject receiver;
    jmethodID runMethodId;
} CborHandler;

GG_Result CborHandler_HandleRequest(
        GG_RemoteCborHandler *self,
        const char* request_method,
        GG_Buffer *request_params,
        GG_JsonRpcErrorCode*  rpc_error_code,
        GG_Buffer **response_params
);

GG_IMPLEMENT_INTERFACE(RunnableCborHandler, GG_RemoteCborHandler) {
        CborHandler_HandleRequest
};

typedef struct {
    GG_IMPLEMENTS(GG_RemoteTransport);
    jobject webSocketTransportObject; //Java side of this object.
    jmethodID sendMethodId;
    jmethodID receiveMethodId;
} WebSocketTransport;

GG_Result WebSocketTransport_Send(GG_RemoteTransport* self, GG_Buffer* data);

GG_Result WebSocketTransport_Receive(GG_RemoteTransport* self, GG_Buffer** data);

GG_IMPLEMENT_INTERFACE(WebSocketTransport, GG_RemoteTransport) {
        WebSocketTransport_Send,
        WebSocketTransport_Receive
};

}
#endif //GOLDENGATE_JNI_GG_REMOTE_H

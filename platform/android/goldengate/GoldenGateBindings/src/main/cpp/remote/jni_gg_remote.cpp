#include <xp/common/gg_common.h>
#include "jni_gg_remote.h"
#include <logging/jni_gg_logging.h>
#include <jni_gg_loop.h>
#include <util/jni_gg_utils.h>


extern "C" {

JNIEXPORT jlong
JNICALL Java_com_fitbit_goldengate_bindings_remote_RemoteShellThread_createJNI(
        JNIEnv * env ,
        jobject thiz,/* this */
        jlong transportPtr
) {
    GG_RemoteShell* shell = NULL;
    GG_RemoteTransport* transport = (GG_RemoteTransport*) (intptr_t) transportPtr;
    GG_Result result = GG_RemoteShell_Create(transport, &shell);

    if (GG_FAILED(result)) {
        GG_Log_JNI("RemoteAPI", "GG_RemoteShell_Create failed with error code %d", result);
    }

    return (jlong) (intptr_t) shell;
}

JNIEnv* remoteShellEnv; // Global

JNIEXPORT void
JNICALL Java_com_fitbit_goldengate_bindings_remote_RemoteShellThread_runJNI(
        JNIEnv * env ,
        jobject thiz/* this */,
        jlong shellPtr) {
    GG_RemoteShell* shell = (GG_RemoteShell*) (intptr_t) shellPtr;
    remoteShellEnv = env;
    GG_RemoteShell_Run(shell);
}

GG_Result CborHandler_HandleRequest(
        GG_RemoteCborHandler *_self,
        const char* request_method,
        GG_Buffer *request_params, // This can be null
        GG_JsonRpcErrorCode*  rpc_error_code,
        GG_Buffer **response_params
) {
    GG_Log_JNI("RemoteAPI", "Handling new request");
    CborHandler* self = GG_SELF(CborHandler, GG_RemoteCborHandler);
    jbyteArray requestByteArray;
    // here we take the data out of request_params, put it in a byte array
    if (request_params) {
        requestByteArray = ggBufferToJByteArray(remoteShellEnv, request_params);
    } else {
        requestByteArray = remoteShellEnv->NewByteArray(0);
    }

    // then call the run method with the byte array
    GG_Log_JNI("RemoteAPI", "Calling back into Java to handle the request");
    jbyteArray responseByteArray = (jbyteArray) remoteShellEnv->CallObjectMethod(
            self->receiver,
            self->runMethodId,
            requestByteArray);
    if (remoteShellEnv->ExceptionCheck()) {
        jthrowable exception = remoteShellEnv->ExceptionOccurred();
        remoteShellEnv->ExceptionClear();
        remoteShellEnv->DeleteLocalRef(responseByteArray);
        int errorCode = GG_ERROR_INTERNAL;
        if (checkObjectIsOfTypeClass(remoteShellEnv, exception, "java.lang.IllegalArgumentException")) {
            errorCode = GG_ERROR_INVALID_PARAMETERS;
        } else {
            GG_Log_JNI("RemoteAPI", "Handler threw an Exception");
            printStackTrace(remoteShellEnv, exception);
        }
        remoteShellEnv->DeleteLocalRef(exception);
        return errorCode;
    }
    remoteShellEnv->DeleteLocalRef(requestByteArray);

    if (responseByteArray == NULL) {
        GG_Log_JNI("RemoteAPI", "Handler returned null, replying with error response");
        return GG_ERROR_INTERNAL;
    }

    GG_Log_JNI("RemoteAPI", "Parsing and returning the response");
    // get the returned byte array and put it in a GG_Buffer to fill in response_params
    GG_Result result = jbyteArrayToGG_Buffer(remoteShellEnv, responseByteArray, response_params);

    remoteShellEnv->DeleteLocalRef(responseByteArray);

    return result;
}

JNIEXPORT jlong
JNICALL Java_com_fitbit_goldengate_bindings_remote_RemoteShellThread_registerHandlerJNI(
        JNIEnv* env,
        jobject thiz, /* this */
        jlong shellPtr,
        jstring handlerName,
        jobject receiver) {
    GG_RemoteShell* shell = (GG_RemoteShell*) (intptr_t) shellPtr;
    const char* handlerNameCString = env->GetStringUTFChars(handlerName, NULL);

    CborHandler* handler = (CborHandler *) GG_AllocateZeroMemory(sizeof(CborHandler));
    GG_SET_INTERFACE(handler, RunnableCborHandler, GG_RemoteCborHandler);

    // register an object (i.e. keep a receiver pointer around)
    jobject receiverGlobal = env->NewGlobalRef(receiver);
    handler->receiver = receiverGlobal;
    jclass receiverClass = env->GetObjectClass(receiver);
    handler->runMethodId = env->GetMethodID(receiverClass, "handle", "([B)[B");

    GG_RemoteCborHandler* remoteCborHandler = GG_CAST(handler, GG_RemoteCborHandler);

    GG_Log_JNI("RemoteAPI", "Registering Handler");
    GG_Result result = GG_RemoteShell_RegisterCborHandler(shell, handlerNameCString, remoteCborHandler);

    if (GG_FAILED(result)) {
        GG_Log_JNI("RemoteAPI", "Registering handler failed with result %d", result);
    }
    // cleanup
    env->ReleaseStringUTFChars(handlerName, handlerNameCString);
    return (jlong) (intptr_t) handler;
}

JNIEXPORT void
JNICALL Java_com_fitbit_goldengate_bindings_remote_RemoteShellThread_freeHandlerJNI(
        JNIEnv* env,
        jobject thiz, /* this */
        jlong handlerPtr) {
    GG_Log_JNI("RemoteAPI", "Freeing handler %d", handlerPtr);
    CborHandler* handler = (CborHandler*) (intptr_t) handlerPtr;
    env->DeleteGlobalRef(handler->receiver);
    GG_ClearAndFreeObject(handler, 1);
}

JNIEXPORT jlong
JNICALL Java_com_fitbit_goldengate_bindings_remote_WebSocketTransport_createJNI(
        JNIEnv* env,
        jobject thiz /*this*/
) {
    WebSocketTransport* transport = (WebSocketTransport *) GG_AllocateZeroMemory(sizeof(WebSocketTransport));
    GG_SET_INTERFACE(transport, WebSocketTransport, GG_RemoteTransport);

    transport->webSocketTransportObject = env->NewGlobalRef(thiz);
    jclass clazz = env->GetObjectClass(thiz);
    transport->sendMethodId = env->GetMethodID(clazz, "send", "([B)Z");
    transport->receiveMethodId = env->GetMethodID(clazz, "receive", "()[B");
    return (jlong) (intptr_t) transport;
}

JNIEXPORT void
JNICALL Java_com_fitbit_goldengate_bindings_remote_WebSocketTransport_destroyJNI(
        JNIEnv* env,
        jobject thiz, /*this*/
        jlong transportPtr
) {
    WebSocketTransport* transport = (WebSocketTransport*) (intptr_t) transportPtr;
    env->DeleteGlobalRef(transport->webSocketTransportObject);
    GG_FreeMemory(transport);
}

// Will be called from Remote Shell thread
GG_Result WebSocketTransport_Send(GG_RemoteTransport* _self, GG_Buffer* data) {
    GG_Log_JNI("WebSocketTransport", "Sending message on websocket");
    WebSocketTransport* self = GG_SELF(WebSocketTransport, GG_RemoteTransport);
    jbyteArray dataByteArray = ggBufferToJByteArray(remoteShellEnv, data);

    jboolean jResult = remoteShellEnv->CallBooleanMethod(
            self->webSocketTransportObject,
            self->sendMethodId,
            dataByteArray);

    remoteShellEnv->DeleteLocalRef(dataByteArray);
    GG_Result result;
    if (jResult) {
        result = GG_SUCCESS;
    } else {
        result = GG_FAILURE;
    }
    return result;
}

// Will be called from Remote Shell thread
GG_Result WebSocketTransport_Receive(GG_RemoteTransport* _self, GG_Buffer** data) {

    WebSocketTransport* self = GG_SELF(WebSocketTransport, GG_RemoteTransport);

    GG_Log_JNI("WebSocketTransport", "About to call back to Java to get data from WebSocket");
    // This method blocks until it has data for us.
    jbyteArray receivedData = (jbyteArray) remoteShellEnv->CallObjectMethod(
            self->webSocketTransportObject,
            self->receiveMethodId);

    GG_Log_JNI("WebSocketTransport", "Got Data from Java side");

    size_t receivedSize = (size_t) remoteShellEnv->GetArrayLength(receivedData);

    if (receivedSize == 0) {
        GG_Log_JNI("WebSocketTransport", "Data was an empty array, exiting");
        return GG_ERROR_REMOTE_EXIT;
    }

    return jbyteArrayToGG_Buffer(remoteShellEnv, receivedData, data);
}


}

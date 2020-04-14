//
// Created by abranscomb on 3/30/18.
//

#include "jni_gg_stackbuider.h"
#include <xp/stack_builder/gg_stack_builder.h>
#include <cstring>
#include <xp/common/gg_utils.h>
#include <cstdio>
#include "jni.h"
#include <xp/common/gg_memory.h>
#include <sockets/jni_gg_socket_address.h>

#include "../logging/jni_gg_logging.h"
#include "../jni_gg_loop.h"
#include "../coap/jni_gg_coap_common.h"

#include <xp/common/gg_io.h>
#include <xp/common/gg_results.h>
#include <xp/tls/gg_tls.h>
#include <xp/common/gg_events.h>
#include <util/jni_gg_utils.h>
#include <xp/stack_builder/gg_stack_builder_base.h>

extern "C" {

typedef struct {
    GG_IMPLEMENTS(GG_TlsKeyResolver);
    jobject node_key;
    jobject tls_key_resolver;
} KeyResolver;

// Structure holding an event listener and what to callback on
typedef struct {
    GG_IMPLEMENTS(GG_EventListener);
    jobject receiver;
    jmethodID dtlsCallback;
    jmethodID stackEventCallback;
} EventListener;

// Structure holding a stack and its event listener
typedef struct {
    GG_Stack *stack;
    EventListener *eventListener;
    KeyResolver *key_resolver;
    GG_DataSource* transport_source;
} StackWrapper;

typedef struct {
    const char *descriptor;
    const GG_StackBuilderParameters *parameters;
    size_t parameterCount;
    GG_StackRole role;
    GG_StackIpConfiguration *ipConfiguration;
    GG_DataSource *transportSource;
    GG_DataSink *transportSink;
    StackWrapper *stackWrapper;
} BuildStackArgs;

/********
 * DTLS *
 ********/
#define GG_STACK_DTLS_KEY_SIZE 16

typedef struct Psk Psk;
struct Psk {
    const uint8_t *identity;
    size_t identity_size;
    uint8_t key[GG_STACK_DTLS_KEY_SIZE];
    Psk *next;
};

/**
 * Resolve a key for given key identity.
 *
 * @param node_key key for connected Node
 * @param tls_key_resolver java/kotlin resolver object reference
 * @param key_identity Identity of the key to resolve.
 * @param key_identity_size Size of the key identity.
 * @return resolved key if found, NULL otherwise
 */
static jbyteArray resolve(
        jobject node_key,
        jobject tls_key_resolver,
        const uint8_t *key_identity,
        size_t key_identity_size
) {
    GG_ASSERT(tls_key_resolver);

    JNIEnv *env = Loop_GetJNIEnv();

    jbyteArray _key_identity = JByteArray_From_DataPointer(env, (jbyte *) key_identity, (jsize) key_identity_size);
    GG_ASSERT(_key_identity);

    jclass tls_key_resolver_class = env->FindClass(TLS_KEY_RESOLVER_CLASS_NAME);
    GG_ASSERT(tls_key_resolver_class);

    jmethodID tls_key_resolver_resolve_id = env->GetMethodID(
            tls_key_resolver_class,
            TLS_KEY_RESOLVER_RESOLVE_NAME,
            TLS_KEY_RESOLVER_RESOLVE_SIG);
    GG_ASSERT(tls_key_resolver_resolve_id);

    jbyteArray key_byte_array = (jbyteArray) env->CallObjectMethod(
            tls_key_resolver,
            tls_key_resolver_resolve_id,
            node_key,
            _key_identity);

    env->DeleteLocalRef(tls_key_resolver_class);
    env->DeleteLocalRef(_key_identity);

    return key_byte_array;
}

static GG_Result
TlsKeyResolver_ResolvePsk(
        GG_TlsKeyResolver *_self,
        const uint8_t *key_identity,
        size_t key_identity_size,
        uint8_t *key,
        size_t *key_size
) {
    KeyResolver *self = GG_SELF(KeyResolver, GG_TlsKeyResolver);
    GG_ASSERT(self);

    // we only support 16-byte keys
    if (*key_size < GG_STACK_DTLS_KEY_SIZE) {
        return GG_ERROR_NOT_ENOUGH_SPACE;
    }

    // delegate to kotlin to resolve key
    jbyteArray key_byte_array = resolve(
            self->node_key,
            self->tls_key_resolver,
            key_identity,
            key_identity_size);
    if (key_byte_array) {
        // set [out] params
        JNIEnv *env = Loop_GetJNIEnv();
        *key_size = (size_t) env->GetArrayLength(key_byte_array);
        jbyte *_key = env->GetByteArrayElements(key_byte_array, NULL);
        memcpy(key, _key, *key_size);

        env->ReleaseByteArrayElements(key_byte_array, _key, JNI_ABORT);
        env->DeleteLocalRef(key_byte_array);

        GG_Log_JNI("Stack", "Key resolved for given keyID");
        return GG_SUCCESS;
    } else {
        GG_Log_JNI("Stack", "Key NOT resolved for given keyID");
        return GG_ERROR_NO_SUCH_ITEM;
    }
}

GG_IMPLEMENT_INTERFACE(TlsKeyResolver, GG_TlsKeyResolver) {
        TlsKeyResolver_ResolvePsk
};

/******************
 * Stack listener *
 ******************/

/**
 * Utility method converting 4 bytes to a string.
 * @param code Code to be converted
 * @param string Reference to char arrach in which this will be put
 */
static void
Convert4CCToString(uint32_t code, char *string) {
    snprintf(string, 5, "%c%c%c%c",
             (char) (code >> 24),
             (char) (code >> 16),
             (char) (code >> 8),
             (char) (code));
}

/**
 * Utility method that will call back into Kotlin when a TLS event is received
 * @param listener The listener containing the receiver (object instance) and method to call
 * @param tlsState An integer representing the state
 * @param tlsLastError An integer representing the TLS error, if applicable
 */
static void onTlsEvent(EventListener *listener, int tlsState, int tlsLastError,
                       const uint8_t* pskIdentity, int pskIdentitySize) {
    JNIEnv *env = Loop_GetJNIEnv();

    jbyteArray pskIdentityByteArray = env->NewByteArray(pskIdentitySize);
    env->SetByteArrayRegion(pskIdentityByteArray,
                            0,
                            pskIdentitySize,
                            (jbyte *)pskIdentity);

    env->CallVoidMethod(listener->receiver,
                        listener->dtlsCallback,
                        tlsState,
                        tlsLastError,
                        pskIdentityByteArray);

    env->DeleteLocalRef(pskIdentityByteArray);
}

/**
 * Utility method that will call back into Kotlin when a stack event is received
 * @param listener The listener containing the receiver (object instance) and method to call
 * @param eventId An event ID type within enum [StackEvent]
 * @param data The extra optional data field
 */
static void onStackEvent(EventListener *listener, int eventId, int data) {
    JNIEnv *env = Loop_GetJNIEnv();
    env->CallVoidMethod(listener->receiver,
                        listener->stackEventCallback,
                        eventId,
                        data);
}

static void
StackListener_OnEvent(GG_EventListener *_self, const GG_Event *event) {
    GG_COMPILER_UNUSED(_self);
    EventListener *self = GG_SELF(EventListener, GG_EventListener);

    char type_str[5];
    Convert4CCToString(event->type, type_str);
    if (event->type == GG_EVENT_TYPE_STACK_EVENT_FORWARD) {
        const GG_StackForwardEvent *forward_event = (const GG_StackForwardEvent *) event;
        Convert4CCToString(forward_event->forwarded->type, type_str);

        switch (forward_event->forwarded->type) {
            case GG_EVENT_TYPE_TLS_STATE_CHANGE: {
                GG_DtlsProtocolStatus dtls_status;
                GG_DtlsProtocol *dtls_protocol = (GG_DtlsProtocol *) forward_event->forwarded->source;
                GG_DtlsProtocol_GetStatus(dtls_protocol, &dtls_status);
                onTlsEvent(self, dtls_status.state, dtls_status.last_error,
                           dtls_status.psk_identity, (int)dtls_status.psk_identity_size);
                break;
            }

            default:
                // If the event is not a dtls event we just forward it to a Kotlin handler.
                uint32_t data = 0;
                if (forward_event->forwarded->type == GG_EVENT_TYPE_GATTLINK_SESSION_STALLED) {
                    GG_GattlinkStalledEvent* stalled_event = (GG_GattlinkStalledEvent*)forward_event->forwarded;
                    data = stalled_event->stalled_time;
                }
                onStackEvent(self, forward_event->forwarded->type, data);
                Convert4CCToString(forward_event->forwarded->type, type_str);
                GG_Log_JNI("Stack", "Event type received: %s", type_str);
                break;
        }
    }
}

GG_IMPLEMENT_INTERFACE(StackListener, GG_EventListener) {
        .OnEvent = StackListener_OnEvent
};

/*********
 * Stack *
 *********/

GG_Result BuildStack(void *buildStackArgs) {
    BuildStackArgs *args = (BuildStackArgs *) buildStackArgs;
    GG_Result result = GG_StackBuilder_BuildStack(
            args->descriptor,
            args->parameters,
            args->parameterCount,
            args->role,
            args->ipConfiguration,
            Loop_GetLoop(),
            args->transportSource,
            args->transportSink,
            &args->stackWrapper->stack);
    return result;
}

static jobject Stack_Create_Response_Object_From_Values(
        JNIEnv *env,
        GG_Result result,
        StackWrapper *_stack_wrapper
) {
    jclass stack_creation_result_class = env->FindClass(
            STACK_CREATION_RESULT_CLASS_NAME);
    GG_ASSERT(stack_creation_result_class);

    jmethodID stack_creation_result_constructor_id = env->GetMethodID(
            stack_creation_result_class,
            CONSTRUCTOR_NAME,
            STACK_CREATION_RESULT_CONSTRUCTOR_SIG);
    GG_ASSERT(stack_creation_result_constructor_id);

    jint result_code = (jint) result;
    jlong stack_wrapper = (jlong) (intptr_t) _stack_wrapper;

    jobject stack_creation_result_object = env->NewObject(
            stack_creation_result_class,
            stack_creation_result_constructor_id,
            result_code,
            stack_wrapper);
    GG_ASSERT(stack_creation_result_object);

    env->DeleteLocalRef(stack_creation_result_class);

    return stack_creation_result_object;
}

JNIEXPORT jobject
JNICALL Java_com_fitbit_goldengate_bindings_stack_Stack_create(
        JNIEnv *env,
        jobject thiz/* this */,
        jobject nodeKey,
        jstring stackDescriptorString,
        jboolean isNode,
        jlong transportSinkPointer,
        jlong transportSourcePointer,
        jobject localAddress,
        jint localPortNumber,
        jobject remoteAddress,
        jint remotePortNumber,
        jobject tlsKeyResolver
) {
    GG_ASSERT(nodeKey);
    GG_ASSERT(tlsKeyResolver);

    // Create the wrapper that will hold the stack and the callbacks
    StackWrapper *stackWrapper = (StackWrapper *) GG_AllocateZeroMemory(sizeof(StackWrapper));

    // Keep a reference to the transport so that we can detach from it later
    stackWrapper->transport_source = (GG_DataSource*)(intptr_t)transportSourcePointer;

    // IP stack configuration
    GG_StackIpConfiguration *ipConfiguration = NULL;
    GG_IpAddress local_address = GG_IpAddress_From_Inet4Address_Object(env, localAddress);
    GG_IpAddress remote_address = GG_IpAddress_From_Inet4Address_Object(env, remoteAddress);

    if (GG_IpAddress_AsInteger(&local_address) != 0 ||
        GG_IpAddress_AsInteger(&remote_address) != 0) {
        ipConfiguration = (GG_StackIpConfiguration *) GG_AllocateZeroMemory(sizeof(GG_StackIpConfiguration));
        if (ipConfiguration) {
            ipConfiguration->local_address = local_address;
            ipConfiguration->remote_address = remote_address;
        }
    } else {
        // this is required to use default ip configuration to support multiple gg devices
        GG_Log_JNI("Stack", "use default ip configuration");
    }

    // Prepare arguments
    BuildStackArgs *args = (BuildStackArgs *) GG_AllocateZeroMemory(sizeof(BuildStackArgs));
    args->descriptor = env->GetStringUTFChars(stackDescriptorString, NULL);
    args->role = isNode ? GG_STACK_ROLE_NODE : GG_STACK_ROLE_HUB;
    args->ipConfiguration = ipConfiguration;
    args->transportSource = (GG_DataSource *) (intptr_t) transportSourcePointer;
    args->transportSink = (GG_DataSink *) (intptr_t) transportSinkPointer;

    // Parameters
    GG_StackBuilderParameters parameters[3];
    size_t parameter_count = 0;
    args->parameters = parameters;
    union {
        GG_TlsClientOptions clientOptions;
        GG_TlsServerOptions serverOptions;
    } options;

    uint16_t cipher_suites[3] = {
            GG_TLS_PSK_WITH_AES_128_CCM,
            GG_TLS_PSK_WITH_AES_128_GCM_SHA256,
            GG_TLS_ECDHE_PSK_WITH_AES_128_CBC_SHA256
    };

    // Gattlink probe config
    GG_GattlinkProbeConfig probeConfig;
    probeConfig.window_size_ms = GATTLINK_PROBE_DEFAULT_WINDOW_SPAN;
    probeConfig.buffer_sample_count = GATTLINK_PROBE_DEFAULT_BUFFER_SAMPLE_COUNT;
    probeConfig.buffer_threshold = GATTLINK_PROBE_DEFAULT_BUFFER_USAGE_THRESHOLD;
    GG_StackElementGattlinkParameters gl_config = {
            .rx_window = 0,
            .tx_window = 0,
            .buffer_size = 0,
            .initial_max_fragment_size = GG_STACK_BUILDER_DEFAULT_GATTLINK_FRAGMENT_SIZE,
            .probe_config = &probeConfig,
    };
    parameters[parameter_count].element_type = GG_STACK_ELEMENT_TYPE_GATTLINK;
    parameters[parameter_count].element_parameters = &gl_config;
    parameter_count++;

    if (strchr(args->descriptor, 'D')) {
        // TODO: We are no longer supporting mobile as Node, this assert is to minimize removal of
        // code for supporting running mobile as Node
        GG_ASSERT(isNode == JNI_FALSE);

        KeyResolver *key_resolver = (KeyResolver *) GG_AllocateZeroMemory(sizeof(KeyResolver));
        key_resolver->node_key = env->NewGlobalRef(nodeKey);
        key_resolver->tls_key_resolver = env->NewGlobalRef(tlsKeyResolver);
        stackWrapper->key_resolver = key_resolver;

        GG_SET_INTERFACE(key_resolver, TlsKeyResolver, GG_TlsKeyResolver);

        options.serverOptions = {
                .base = {
                        .cipher_suites       = cipher_suites,
                        .cipher_suites_count = GG_ARRAY_SIZE(cipher_suites)
                },
                .key_resolver = GG_CAST(key_resolver, GG_TlsKeyResolver)
        };
        parameters[parameter_count].element_type = GG_STACK_ELEMENT_TYPE_DTLS_SERVER;
        parameters[parameter_count].element_parameters = &options.serverOptions;

        parameter_count++;
    }

    GG_StackElementDatagramSocketParameters socketParameters;
    if (localPortNumber != 0 || remotePortNumber != 0) {
        socketParameters = {
                .local_port = static_cast<uint16_t>(localPortNumber),
                .remote_port = static_cast<uint16_t>(remotePortNumber)
        };

        parameters[parameter_count].element_type = GG_STACK_ELEMENT_TYPE_DATAGRAM_SOCKET;
        parameters[parameter_count].element_parameters = &socketParameters;
        parameter_count++;
    }

    args->parameterCount = parameter_count;

    args->stackWrapper = stackWrapper;

    // Build the stack
    GG_Result result;
    Loop_InvokeSync(BuildStack, args, &result);

    // Clean up
    env->ReleaseStringUTFChars(stackDescriptorString, args->descriptor);
    if (ipConfiguration) {
        GG_FreeMemory(ipConfiguration);
    }
    GG_FreeMemory(args);

    return Stack_Create_Response_Object_From_Values(env, result, stackWrapper);
}

GG_Result AttachEventListener(void *_args) {
    StackWrapper *stackWrapper = (StackWrapper *) _args;

    GG_SET_INTERFACE(stackWrapper->eventListener, StackListener, GG_EventListener);
    GG_EventEmitter_SetListener(
            GG_Stack_AsEventEmitter(stackWrapper->stack),
            GG_CAST(stackWrapper->eventListener, GG_EventListener));

    return GG_SUCCESS;
}

JNIEXPORT jint
JNICALL Java_com_fitbit_goldengate_bindings_stack_Stack_attachEventListener(
        JNIEnv *env,
        jobject thiz/* this */,
        jclass clazz,
        jlong stackWrapperPtr
) {
    StackWrapper *stackWrapper = (StackWrapper *) (intptr_t) stackWrapperPtr;
    GG_ASSERT(stackWrapper);
    GG_ASSERT(stackWrapper->stack);

    // DTLS event listener
    jmethodID dtlsCallback = env->GetMethodID(clazz, "onDtlsStatusChange", "(II[B)V");
    GG_ASSERT(dtlsCallback);

    // Link controller callback
    jmethodID stackEventCallback = env->GetMethodID(clazz, "onStackEvent", "(II)V");
    GG_ASSERT(stackEventCallback);

    jobject globalRefObject = env->NewGlobalRef(thiz);
    assert(stackWrapper->eventListener == NULL);
    stackWrapper->eventListener = (EventListener *) (intptr_t) GG_AllocateZeroMemory(
            sizeof(EventListener));
    stackWrapper->eventListener->dtlsCallback = dtlsCallback;
    stackWrapper->eventListener->stackEventCallback = stackEventCallback;
    stackWrapper->eventListener->receiver = globalRefObject;

    GG_Result result;
    Loop_InvokeSync(AttachEventListener, stackWrapper, &result);

    return result;
}

GG_Result DestroyStack(void* args) {
    StackWrapper* stackWrapper = (StackWrapper*)args;

    // detach the source
    if (stackWrapper->transport_source) {
        GG_DataSource_SetDataSink(stackWrapper->transport_source, NULL);
    }

    // remove the listener
    GG_EventEmitter_SetListener(GG_Stack_AsEventEmitter(stackWrapper->stack), NULL);

    // destroy the object
    GG_Stack_Destroy(stackWrapper->stack);

    return GG_SUCCESS;
}

JNIEXPORT void
JNICALL Java_com_fitbit_goldengate_bindings_stack_Stack_destroy(
        JNIEnv *env,
        jobject thiz/* this */,
        jlong stackWrapperPtr
) {
    StackWrapper *stackWrapper = (StackWrapper *) (intptr_t) stackWrapperPtr;

    GG_Result result;
    Loop_InvokeSync(DestroyStack, stackWrapper, &result);
    env->DeleteGlobalRef(stackWrapper->eventListener->receiver);
    if (stackWrapper->eventListener != 0) {
        GG_FreeMemory(stackWrapper->eventListener);
    }
    if (stackWrapper->key_resolver != 0) {
        env->DeleteGlobalRef(stackWrapper->key_resolver->node_key);
        env->DeleteGlobalRef(stackWrapper->key_resolver->tls_key_resolver);
        GG_FreeMemory(stackWrapper->key_resolver);
    }
    GG_ClearAndFreeObject(stackWrapper, 0);
}

GG_Result StartStack(void *stackArg) {
    GG_Stack *stack = (GG_Stack *) stackArg;
    return GG_Stack_Start(stack);
}

JNIEXPORT jint
JNICALL Java_com_fitbit_goldengate_bindings_stack_Stack_start(
        JNIEnv *env,
        jobject thiz/* this */,
        jlong stackWrapperPtr
) {
    StackWrapper *stackWrapper = (StackWrapper *) (intptr_t) stackWrapperPtr;
    GG_Stack *stack = stackWrapper->stack;
    GG_Result result;
    Loop_InvokeSync(StartStack, stack, &result);
    return result;
}

typedef struct {
    StackWrapper *stackWrapper;
    uint16_t mtu;
} UpdateMtuArgs;

GG_Result UpdateStackMtu(void *_args) {
    UpdateMtuArgs *updateMtuArgs = (UpdateMtuArgs *) _args;
    GG_StackLinkMtuChangeEvent event;
    event.base.type = GG_EVENT_TYPE_LINK_MTU_CHANGE;
    event.base.source = NULL;
    event.link_mtu = updateMtuArgs->mtu;
    GG_EventListener_OnEvent(GG_Stack_AsEventListener(updateMtuArgs->stackWrapper->stack),
                             &event.base);
    return GG_SUCCESS;
}

JNIEXPORT jboolean
JNICALL Java_com_fitbit_goldengate_bindings_stack_Stack_updateMtu(
        JNIEnv *env,
        jobject thiz/* this */,
        jint mtu,
        jlong stackWrapperPtr
) {

    UpdateMtuArgs updateMtuArgs;
    updateMtuArgs.stackWrapper = (StackWrapper *) (intptr_t) stackWrapperPtr;
    updateMtuArgs.mtu = static_cast<uint16_t>(mtu);

    GG_Result result;
    Loop_InvokeSync(UpdateStackMtu, &updateMtuArgs, &result);
    return (jboolean) GG_SUCCEEDED(result);
}

/*****************
 * Stack utility *
 *****************/

typedef struct {
    GG_Stack *in_stack;
    GG_StackElementPortInfo *out_port_info;
} GetPortInfoArgs;

GG_Result GetTopPortInfo(void *_args) {
    GetPortInfoArgs *args = (GetPortInfoArgs *) _args;
    GG_StackElementInfo elementInfo;
    GG_Stack_GetElementByIndex(args->in_stack, 0, &elementInfo);
    GG_Stack_GetPortById(args->in_stack, elementInfo.id, GG_STACK_PORT_ID_TOP, args->out_port_info);

    return GG_SUCCESS;
}

JNIEXPORT jlong
JNICALL Java_com_fitbit_goldengate_bindings_stack_Stack_getTopPortAsDataSink(
        JNIEnv *env,
        jobject thiz/* this */,
        jlong stackWrapperPtr
) {
    StackWrapper *stackWrapper = (StackWrapper *) (intptr_t) stackWrapperPtr;
    GG_Stack *stack = stackWrapper->stack;
    GetPortInfoArgs *args = (GetPortInfoArgs *) GG_AllocateZeroMemory(sizeof(GetPortInfoArgs));
    args->in_stack = stack;
    GG_StackElementPortInfo *portInfo = (GG_StackElementPortInfo *)
            GG_AllocateZeroMemory(sizeof(GG_StackElementPortInfo));
    args->out_port_info = portInfo;
    GG_Result result;
    Loop_InvokeSync(GetTopPortInfo, args, &result);
    GG_DataSink *sink = args->out_port_info->sink;

    GG_FreeMemory(portInfo);
    GG_FreeMemory(args);

    return (jlong) (intptr_t) sink;
}

JNIEXPORT jlong
JNICALL Java_com_fitbit_goldengate_bindings_stack_Stack_getTopPortAsDataSource(
        JNIEnv *env,
        jobject thiz/* this */,
        jlong stackWrapperPtr
) {
    StackWrapper *stackWrapper = (StackWrapper *) (intptr_t) stackWrapperPtr;
    GG_Stack *stack = stackWrapper->stack;
    GetPortInfoArgs *args = (GetPortInfoArgs *) GG_AllocateZeroMemory(sizeof(GetPortInfoArgs));
    args->in_stack = stack;
    GG_StackElementPortInfo *portInfo = (GG_StackElementPortInfo *)
            GG_AllocateZeroMemory(sizeof(GG_StackElementPortInfo));
    args->out_port_info = portInfo;
    GG_Result result;
    Loop_InvokeSync(GetTopPortInfo, args, &result);
    GG_DataSource *source = args->out_port_info->source;

    GG_FreeMemory(portInfo);
    GG_FreeMemory(args);

    return (jlong) (intptr_t) source;
}

}

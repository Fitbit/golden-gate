/**
 *
 * @file
 *
 * @copyright
 * Copyright 2017-2019 by Fitbit, Inc., all rights reserved.
 *
 * @author Gilles Boccon-Gibod
 *
 * @date 2019-02-21
 *
 * @details
 *
 * Web Bluetooth example
 */

//----------------------------------------------------------------------
// Includes
//----------------------------------------------------------------------
#include <stdio.h>
#include <string.h>
#include <limits.h>

#include <emscripten.h>

#include "xp/module/gg_module.h"
#include "xp/common/gg_common.h"
#include "xp/loop/gg_loop.h"
#include "xp/stack_builder/gg_stack_builder.h"

//----------------------------------------------------------------------
// Logging
//----------------------------------------------------------------------
GG_SET_LOCAL_LOGGER("gg.xp.examples.web-bluetooth")

//----------------------------------------------------------------------
// DTLS key resolver
//----------------------------------------------------------------------
typedef struct Psk Psk;
struct Psk {
    const uint8_t* identity;
    size_t         identity_size;
    uint8_t        key[16];
    Psk*           next;
};

typedef struct {
    GG_IMPLEMENTS(GG_TlsKeyResolver);

    Psk* psks;
} StaticPskResolver;

//----------------------------------------------------------------------
static GG_Result
StaticPskResolver_ResolvePsk(GG_TlsKeyResolver* _self,
                             const uint8_t*     key_identity,
                             size_t             key_identity_size,
                             uint8_t*           key,
                             size_t*            key_size)
{
    StaticPskResolver* self = (StaticPskResolver*)GG_SELF(StaticPskResolver, GG_TlsKeyResolver);

    // we only support 16-byte keys
    if (*key_size < 16) {
        *key_size = 16;
        return GG_ERROR_NOT_ENOUGH_SPACE;
    }

    // look for a match in the list
    Psk* psk = self->psks;
    while (psk) {
        if (key_identity_size == psk->identity_size &&
            !memcmp(key_identity, psk->identity, key_identity_size)) {
            // match! copy the key
            memcpy(key, psk->key, 16);
            *key_size = 16;

            return GG_SUCCESS;
        }

        psk = psk->next;
    }

    return GG_ERROR_NO_SUCH_ITEM;
}

//----------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(StaticPskResolver, GG_TlsKeyResolver) {
    StaticPskResolver_ResolvePsk
};

//----------------------------------------------------------------------
// Constants
//----------------------------------------------------------------------
static uint8_t DefaultKeyIdentity[9] = {
  'B', 'O', 'O', 'T', 'S', 'T', 'R', 'A', 'P'
};
static Psk DefaultPsk = {
    .identity = DefaultKeyIdentity,
    .identity_size = 9,
    .key = {
        0x81, 0x06, 0x54, 0xe3, 0x36, 0xad, 0xca, 0xb0,
        0xa0, 0x3c, 0x60, 0xf7, 0x4a, 0xa0, 0xb6, 0xfb
    },
    .next = NULL
};

//----------------------------------------------------------------------
// Web Bluetooth data source
//----------------------------------------------------------------------
typedef struct {
    GG_IMPLEMENTS(GG_DataSource);

    GG_DataSink* sink;
} WebBluetoothDataSource;

//----------------------------------------------------------------------
static GG_Result
WebBluetoothDataSource_SetDataSink(GG_DataSource* _self, GG_DataSink* sink)
{
    WebBluetoothDataSource* self = GG_SELF(WebBluetoothDataSource, GG_DataSource);

    self->sink = sink;

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(WebBluetoothDataSource, GG_DataSource) {
    .SetDataSink = WebBluetoothDataSource_SetDataSink
};

//----------------------------------------------------------------------
static void
WebBluetoothDataSource_Init(WebBluetoothDataSource* self)
{
    // init the members
    memset(self, 0, sizeof(*self));

    // setup the interface
    GG_SET_INTERFACE(self, WebBluetoothDataSource, GG_DataSource);
}

//----------------------------------------------------------------------
static int
WebBluetoothDataSource_Send(WebBluetoothDataSource* self, const uint8_t* data, size_t data_size)
{
    GG_LOG_FINE("sending %u bytes to the stack transport", (int)data_size);

    if (!self->sink) {
        GG_LOG_WARNING("no sink, dropping");
        return GG_ERROR_INVALID_STATE;
    }

    // create a buffer to copy the data into
    GG_DynamicBuffer* buffer;
    GG_Result result = GG_DynamicBuffer_Create(data_size, &buffer);
    if (GG_FAILED(result)) {
        return result;
    }

    // copy the data
    GG_DynamicBuffer_SetData(buffer, data, data_size);

    // send the data to the sink
    result = GG_DataSink_PutData(self->sink, GG_DynamicBuffer_AsBuffer(buffer), NULL);
    GG_DynamicBuffer_Release(buffer);

    return result;
}

//----------------------------------------------------------------------
// Web Bluetooth data sink
//----------------------------------------------------------------------
typedef struct {
    GG_IMPLEMENTS(GG_DataSink);

    GG_DataSinkListener* listener;
} WebBluetoothDataSink;

//----------------------------------------------------------------------
EM_JS(void, web_bluetooth_on_stack_transport_data, (const uint8_t* data, uint32_t data_size), {
    onGattlinkRx(data, data_size);
})

//----------------------------------------------------------------------
static GG_Result
WebBluetoothDataSink_PutData(GG_DataSink* _self, GG_Buffer* data, const GG_BufferMetadata* metadata)
{
    GG_COMPILER_UNUSED(metadata);
    GG_COMPILER_UNUSED(_self);

    GG_LOG_FINE("received %u bytes from the stack transport", (int)GG_Buffer_GetDataSize(data));
    web_bluetooth_on_stack_transport_data(GG_Buffer_GetData(data), (uint32_t)GG_Buffer_GetDataSize(data));
    return GG_SUCCESS;
}

//----------------------------------------------------------------------
static GG_Result
WebBluetoothDataSink_SetListener(GG_DataSink* _self, GG_DataSinkListener* listener)
{
    WebBluetoothDataSink* self = GG_SELF(WebBluetoothDataSink, GG_DataSink);

    self->listener = listener;

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(WebBluetoothDataSink, GG_DataSink) {
    .PutData     = WebBluetoothDataSink_PutData,
    .SetListener = WebBluetoothDataSink_SetListener
};

//----------------------------------------------------------------------
static void
WebBluetoothDataSink_Init(WebBluetoothDataSink* self)
{
    // init the members
    memset(self, 0, sizeof(*self));

    // setup the interface
    GG_SET_INTERFACE(self, WebBluetoothDataSink, GG_DataSink);
}

//----------------------------------------------------------------------
// Js Client data source
//----------------------------------------------------------------------
typedef struct {
    GG_IMPLEMENTS(GG_DataSource);

    GG_DataSink* sink;
} JsClientDataSource;

//----------------------------------------------------------------------
static GG_Result
JsClientDataSource_SetDataSink(GG_DataSource* _self, GG_DataSink* sink)
{
    JsClientDataSource* self = GG_SELF(JsClientDataSource, GG_DataSource);

    self->sink = sink;

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(JsClientDataSource, GG_DataSource) {
    .SetDataSink = JsClientDataSource_SetDataSink
};

//----------------------------------------------------------------------
static void
JsClientDataSource_Init(JsClientDataSource* self)
{
    // init the members
    memset(self, 0, sizeof(*self));

    // setup the interface
    GG_SET_INTERFACE(self, JsClientDataSource, GG_DataSource);
}

//----------------------------------------------------------------------
static int
JsClientDataSource_Send(JsClientDataSource* self, const uint8_t* data, size_t data_size)
{
    GG_LOG_FINE("sending %u bytes to the top of the stack", (int)data_size);

    if (!self->sink) {
        GG_LOG_WARNING("no sink, dropping");
        return GG_ERROR_INVALID_STATE;
    }

    // create a buffer to copy the data into
    GG_DynamicBuffer* buffer;
    GG_Result result = GG_DynamicBuffer_Create(data_size, &buffer);
    if (GG_FAILED(result)) {
        return result;
    }

    // copy the data
    GG_DynamicBuffer_SetData(buffer, data, data_size);

    // send the data to the sink
    result = GG_DataSink_PutData(self->sink, GG_DynamicBuffer_AsBuffer(buffer), NULL);
    GG_DynamicBuffer_Release(buffer);

    return result;
}

//----------------------------------------------------------------------
// JS Client data sink
//----------------------------------------------------------------------
typedef struct {
    GG_IMPLEMENTS(GG_DataSink);

    GG_DataSinkListener* listener;
} JsClientDataSink;

//----------------------------------------------------------------------
EM_JS(void, js_client_on_stack_top_data, (const uint8_t* data, uint32_t data_size), {
    onStackDataReceived(data, data_size);
})

//----------------------------------------------------------------------
static GG_Result
JsClientDataSink_PutData(GG_DataSink* _self, GG_Buffer* data, const GG_BufferMetadata* metadata)
{
    GG_COMPILER_UNUSED(metadata);
    GG_COMPILER_UNUSED(_self);

    GG_LOG_FINE("received %u bytes from the top of the stack", (int)GG_Buffer_GetDataSize(data));
    js_client_on_stack_top_data(GG_Buffer_GetData(data), (uint32_t)GG_Buffer_GetDataSize(data));
    return GG_SUCCESS;
}

//----------------------------------------------------------------------
static GG_Result
JsClientDataSink_SetListener(GG_DataSink* _self, GG_DataSinkListener* listener)
{
    JsClientDataSink* self = GG_SELF(JsClientDataSink, GG_DataSink);

    self->listener = listener;

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(JsClientDataSink, GG_DataSink) {
    .PutData     = JsClientDataSink_PutData,
    .SetListener = JsClientDataSink_SetListener
};

//----------------------------------------------------------------------
static void
JsClientDataSink_Init(JsClientDataSink* self)
{
    // init the members
    memset(self, 0, sizeof(*self));

    // setup the interface
    GG_SET_INTERFACE(self, JsClientDataSink, GG_DataSink);
}

//----------------------------------------------------------------------
// Globals
//----------------------------------------------------------------------
static GG_Loop*               g_loop;
static GG_Stack*              g_stack;
static WebBluetoothDataSource g_transport_data_source;
static WebBluetoothDataSink   g_transport_data_sink;
static JsClientDataSource     g_client_data_source;
static JsClientDataSink       g_client_data_sink;

//----------------------------------------------------------------------
EMSCRIPTEN_KEEPALIVE
int
web_bluetooth_initialize(const char* log_config)
{
    printf("=== Golden Gate Web Bluetooth ===\n");

    // init Golden Gate
    GG_Module_Initialize();

    // configure logging
    if (log_config && strlen(log_config)) {
        GG_LogManager_Configure(log_config);
    }

    // setup a loop
    GG_Loop_Create(&g_loop);
    GG_Loop_BindToCurrentThread(g_loop);

    // init the transport data source
    WebBluetoothDataSource_Init(&g_transport_data_source);

    // init the transport data sink
    WebBluetoothDataSink_Init(&g_transport_data_sink);

    // init the client data source
    JsClientDataSource_Init(&g_client_data_source);

    // init the client data sink
    JsClientDataSink_Init(&g_client_data_sink);

    // prepare stack construction parameters
    GG_StackBuilderParameters parameters[4];
    size_t parameter_count = 0;

    // initialize a key resolver
    static uint16_t cipher_suites[3] = {
        GG_TLS_PSK_WITH_AES_128_CCM,
        GG_TLS_PSK_WITH_AES_128_GCM_SHA256,
        GG_TLS_ECDHE_PSK_WITH_AES_128_CBC_SHA256
    };

    // setup a DTLS key resolver
    static StaticPskResolver psk_resolver;
    GG_SET_INTERFACE(&psk_resolver, StaticPskResolver, GG_TlsKeyResolver);
    psk_resolver.psks = &DefaultPsk;

    static GG_TlsServerOptions dtls_server_parameters = {
        .base = {
            .cipher_suites       = cipher_suites,
            .cipher_suites_count = GG_ARRAY_SIZE(cipher_suites)
        },
        .key_resolver = GG_CAST(&psk_resolver, GG_TlsKeyResolver)
    };
    parameters[parameter_count].element_type = GG_STACK_ELEMENT_TYPE_DTLS_SERVER;
    parameters[parameter_count].element_parameters = &dtls_server_parameters;
    ++parameter_count;

    // create a stack
    GG_Result result = GG_StackBuilder_BuildStack("DSNG",
                                                  parameters,
                                                  parameter_count,
                                                  GG_STACK_ROLE_HUB,
                                                  NULL,
                                                  g_loop,
                                                  GG_CAST(&g_transport_data_source, GG_DataSource),
                                                  GG_CAST(&g_transport_data_sink, GG_DataSink),
                                                  &g_stack);
    if (GG_FAILED(result)) {
        fprintf(stderr, "ERROR: failed to build stack (%d)", result);
    }

    // attach the client source and sink to the top of the stack
    GG_StackElementPortInfo top_port;
    result = GG_Stack_GetPortById(g_stack, GG_STACK_ELEMENT_ID_TOP, GG_STACK_PORT_ID_TOP, &top_port);
    if (GG_SUCCEEDED(result) && top_port.source && top_port.sink) {
        GG_DataSource_SetDataSink(top_port.source, GG_CAST(&g_client_data_sink, GG_DataSink));
        GG_DataSource_SetDataSink(GG_CAST(&g_client_data_source, GG_DataSource), top_port.sink);
    } else {
        fprintf(stderr, ">>> stack has no connectable top port\n");
    }

    return 0;
}

//----------------------------------------------------------------------
EMSCRIPTEN_KEEPALIVE
void
web_bluetooth_terminate(void)
{
    GG_Loop_Destroy(g_loop);
    g_loop = NULL;
}

//----------------------------------------------------------------------
EMSCRIPTEN_KEEPALIVE
int
web_bluetooth_do_work(void)
{
    printf("web_bluetooth_do_work\n");
    if (!g_loop) {
        return -1;
    }
    uint32_t next_timer = 0;
    GG_Result result = GG_Loop_DoWork(g_loop, 0, &next_timer);
    if (GG_FAILED(result)) {
        return result;
    }

    // clamp the timer value to avoid signed/unsigned issues or overflows
    if (next_timer > INT_MAX) {
        next_timer = INT_MAX;
    }

    return (int)next_timer;
}

//----------------------------------------------------------------------
EMSCRIPTEN_KEEPALIVE
void
web_bluetooth_start_stack(void)
{
    if (g_stack) {
        GG_Stack_Start(g_stack);
    }
}

//----------------------------------------------------------------------
EMSCRIPTEN_KEEPALIVE
int
web_bluetooth_send_to_transport(const uint8_t* data, size_t data_size)
{
    return WebBluetoothDataSource_Send(&g_transport_data_source, data, data_size);
}

//----------------------------------------------------------------------
EMSCRIPTEN_KEEPALIVE
int
web_bluetooth_send_to_stack_top(const uint8_t* data, size_t data_size)
{
    return JsClientDataSource_Send(&g_client_data_source, data, data_size);
}

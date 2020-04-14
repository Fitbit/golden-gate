//----------------------------------------------------------------------
// Includes
//----------------------------------------------------------------------
#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>

#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"

#define GG_CONFIG_ENABLE_LOGGING

#include "xp/module/gg_module.h"
#include "xp/common/gg_common.h"
#include "xp/loop/gg_loop.h"
#include "xp/stack_builder/gg_stack_builder.h"
#include "xp/sockets/gg_sockets.h"
#include "xp/coap/gg_coap.h"

#include "gg_esp32_ble.h"

GG_SET_LOCAL_LOGGER("gg-example");
static const char *TAG = "gg-example";

//----------------------------------------------------------------------
// Types
//----------------------------------------------------------------------
typedef struct {
    GG_IMPLEMENTS(GG_CoapRequestHandler);
} HelloHandler;

//----------------------------------------------------------------------
// Constants
//----------------------------------------------------------------------
#define GG_GATT_OP_OVERHEAD   3

//----------------------------------------------------------------------
// Globals
//----------------------------------------------------------------------
static GG_Loop*         gg_loop;
static GG_Stack*        gg_stack;
static GG_DataSink*     gg_stack_sink;
static GG_CoapEndpoint* gg_coap_endpoint;
static unsigned int     gg_gattlink_mtu;

//----------------------------------------------------------------------
// CoAP Hello World handler
//----------------------------------------------------------------------
static GG_CoapRequestHandlerResult
HelloHandler_OnRequest(GG_CoapRequestHandler*   _self,
                       GG_CoapEndpoint*         endpoint,
                       const GG_CoapMessage*    request,
                       GG_CoapResponder*        responder,
                       const GG_BufferMetadata* metadata,
                       GG_CoapMessage**         response)
{
    GG_COMPILER_UNUSED(_self);
    GG_COMPILER_UNUSED(responder);
    GG_COMPILER_UNUSED(metadata);

    // get the second part of the path after the 'hello'
    GG_CoapMessageOptionIterator options;
    GG_CoapMessage_InitOptionIterator(request, GG_COAP_MESSAGE_OPTION_URI_PATH, &options);
    GG_CoapMessage_StepOptionIterator(request, &options);
    char payload[128];
    size_t payload_size = 0;
    if (options.option.number && options.option.value.string.length < 32) {
        memcpy(payload, "Hello ", 6);
        memcpy(payload + 6, options.option.value.string.chars, options.option.value.string.length);
        payload_size = 6 + options.option.value.string.length;
    } else {
        memcpy(payload, "Hello, World", 12);
        payload_size = 12;
    }
    payload[payload_size] = 0;

    GG_CoapMessageOptionParam option_param =
        GG_COAP_MESSAGE_OPTION_PARAM_UINT(CONTENT_FORMAT, GG_COAP_MESSAGE_FORMAT_ID_TEXT_PLAIN);
    return GG_CoapEndpoint_CreateResponse(endpoint,
                                          request,
                                          GG_COAP_MESSAGE_CODE_CONTENT,
                                          &option_param,
                                          1,
                                          (const uint8_t*)payload,
                                          payload_size,
                                          response);
}

GG_IMPLEMENT_INTERFACE(HelloHandler, GG_CoapRequestHandler) {
    HelloHandler_OnRequest
};

//----------------------------------------------------------------------
// Helper function for gg_on_mtu_changed
//----------------------------------------------------------------------
static void
gg_on_mtu_changed_(void* arg)
{
    GG_COMPILER_UNUSED(arg);

    ESP_LOGI(TAG, "Updating Gattlink MTU: %u", gg_gattlink_mtu);

    // If we don't have a stack, do nothing now
    if (!gg_stack) {
        return;
    }

    // Notify the stack of the MTU change
    GG_StackLinkMtuChangeEvent event = {
        .base = {
            .type = GG_EVENT_TYPE_LINK_MTU_CHANGE,
            .source = NULL
        },
        // The link MTU is the max payload for a GATT write, so GATT MTU less
        // the GATT operation bytes
        .link_mtu = gg_gattlink_mtu
    };

    GG_EventListener_OnEvent(GG_Stack_AsEventListener(gg_stack), &event.base);
}

//----------------------------------------------------------------------
// Helper function for gg_on_packet_received
//----------------------------------------------------------------------
static void
gg_on_packet_received_(void* arg)
{
    // Send the buffer to the sink
    GG_Buffer* buffer = (GG_Buffer*)arg;
    GG_DataSink_PutData(gg_stack_sink, buffer, NULL);
    GG_Buffer_Release(buffer);
}

//----------------------------------------------------------------------
// Callback invoked when data is received on the Gattlink RX characteristic
//----------------------------------------------------------------------
void
gg_on_packet_received(GG_Buffer* packet)
{
    // Check that we have a loop and data sink to pass this to
    if (!gg_loop || !gg_stack_sink) {
        GG_Buffer_Release(packet);
    }

    // Invoke the buffer processing on the GG loop thread
    GG_Loop_InvokeAsync(gg_loop, gg_on_packet_received_, packet);
}

//----------------------------------------------------------------------
// Callback invoked when the connection MTU is updated
//----------------------------------------------------------------------
void
gg_on_mtu_changed(unsigned int mtu)
{
    // Check that we have a loop
    if (!gg_loop) {
        return;
    }

    // Sanity check
    if (mtu <= GG_GATT_OP_OVERHEAD) {
        return;
    }

    gg_gattlink_mtu = mtu - GG_GATT_OP_OVERHEAD;
    GG_Loop_InvokeAsync(gg_loop, gg_on_mtu_changed_, NULL);
}

//----------------------------------------------------------------------
// Create a new stack
//----------------------------------------------------------------------
static void
gg_create_stack(void)
{
    assert(gg_stack == NULL);

    // setup stack config
    GG_StackBuilderParameters stack_params[1]; // increase capacity if more items are needed
    size_t                    stack_param_count = 0;
    uint16_t cipher_suites[3] = {
        GG_TLS_PSK_WITH_AES_128_CCM,
        GG_TLS_PSK_WITH_AES_128_GCM_SHA256,
        GG_TLS_ECDHE_PSK_WITH_AES_128_CBC_SHA256
    };
    const uint8_t bootstrap_key[16] = {
        0x81, 0x06, 0x54, 0xE3, 0x36, 0xAD, 0xCA, 0xB0,
        0xA0, 0x3C, 0x60, 0xF7, 0x4A, 0xA0, 0xB6, 0xFB
    };
    const uint8_t bootstrap_identity[9] = {
        'B', 'O', 'O', 'T', 'S', 'T', 'R', 'A', 'P'
    };
    GG_TlsClientOptions tls_options = {
        .base = {
            .cipher_suites       = cipher_suites,
            .cipher_suites_count = GG_ARRAY_SIZE(cipher_suites)
        },
        .psk_identity      = bootstrap_identity,
        .psk_identity_size = sizeof(bootstrap_identity),
        .psk               = bootstrap_key,
        .psk_size          = sizeof(bootstrap_key)
    };
    stack_params[stack_param_count].element_type = GG_STACK_ELEMENT_TYPE_DTLS_CLIENT;
    stack_params[stack_param_count].element_parameters = &tls_options;
    ++stack_param_count;

    // create a stack
    GG_LOG_INFO("creating stack");
    GG_Result result = GG_StackBuilder_BuildStack("DSNG",
                                                  stack_params,
                                                  stack_param_count,
                                                  GG_STACK_ROLE_NODE,
                                                  NULL,
                                                  gg_loop,
                                                  NULL,
                                                  NULL,
                                                  &gg_stack);
    GG_ASSERT(GG_SUCCEEDED(result));

    // Get the top of the stack to attach the CoAP endpoint
    GG_StackElementPortInfo stack_top;
    GG_Stack_GetPortById(gg_stack,
                         GG_STACK_ELEMENT_ID_TOP,
                         GG_STACK_PORT_ID_TOP,
                         &stack_top);

    // Attach the CoAP endpoint
    GG_DataSource_SetDataSink(GG_CoapEndpoint_AsDataSource(gg_coap_endpoint), stack_top.sink);
    GG_DataSource_SetDataSink(stack_top.source, GG_CoapEndpoint_AsDataSink(gg_coap_endpoint));

    // Get the bottom of the stack as our transport interface
    GG_StackElementPortInfo stack_bottom;
    GG_Stack_GetPortById(gg_stack,
                         GG_STACK_ELEMENT_ID_BOTTOM,
                         GG_STACK_PORT_ID_BOTTOM,
                         &stack_bottom);
    gg_stack_sink = stack_bottom.sink;

    // Attach BLE
    gg_esp32_ble_attach_stack(stack_bottom.source);
}

//----------------------------------------------------------------------
// Destroy the stack
//----------------------------------------------------------------------
static void
gg_destroy_stack(void)
{
    // Detach the CoAP endpoint
    GG_DataSource_SetDataSink(GG_CoapEndpoint_AsDataSource(gg_coap_endpoint), NULL);

    // Detach BLE
    gg_stack_sink = NULL;

    // Destroy the stack
    GG_LOG_INFO("destroying stack");
    GG_Stack_Destroy(gg_stack);
    gg_stack = NULL;
}

//----------------------------------------------------------------------
// Helper function for gg_on_link_up
//----------------------------------------------------------------------
static void
gg_on_link_up_(void* arg)
{
    GG_COMPILER_UNUSED(arg);

    // Create the stack
    ESP_LOGI(TAG, "Creating the stack");
    gg_create_stack();

    // If we have a pending MTU change, make it effective now
    gg_on_mtu_changed_(NULL);

    // Start the stack
    ESP_LOGI(TAG, "Starting the stack");
    GG_Stack_Start(gg_stack);
}


//----------------------------------------------------------------------
// Function called when the Bluetooth link is up
//--------------------------------------------------------------------
void
gg_on_link_up(void)
{
    GG_Loop_InvokeAsync(gg_loop, gg_on_link_up_, NULL);
}

//----------------------------------------------------------------------
// Helper function for gg_on_link_down
//----------------------------------------------------------------------
static void
gg_on_link_down_(void* arg)
{
    GG_COMPILER_UNUSED(arg);

    // Destroy the stack
    ESP_LOGI(TAG, "Destroying the stack");
    gg_destroy_stack();

    // Clear the MTU cache
    gg_gattlink_mtu = 0;
}

//----------------------------------------------------------------------
// Function called when the Bluetooth link is down
//----------------------------------------------------------------------
void
gg_on_link_down(void)
{
    GG_Loop_InvokeAsync(gg_loop, gg_on_link_down_, NULL);
}

//-----------------------------------------------------
void
app_main(void)
{
    ESP_LOGI(TAG, "Hello Golden Gate");

    // Init ESP32 subsystems
    ESP_ERROR_CHECK(nvs_flash_init());

    // Init Golden Gate
    GG_Module_Initialize();

    GG_LogManager_Configure("plist:.level=INFO");

    // Create a loop
    GG_Result result = GG_Loop_Create(&gg_loop);

    // Create a CoAP endpoint (not attached to any stack yet)
    result = GG_CoapEndpoint_Create(GG_Loop_GetTimerScheduler(gg_loop),
                                    NULL,
                                    NULL,
                                    &gg_coap_endpoint);

    // Create and register the HelloWorld handler
    HelloHandler hello_handler;
    GG_SET_INTERFACE(&hello_handler, HelloHandler, GG_CoapRequestHandler);
    GG_CoapEndpoint_RegisterRequestHandler(gg_coap_endpoint,
                                           "hello",
                                           GG_COAP_REQUEST_HANDLER_FLAG_ALLOW_GET,
                                           GG_CAST(&hello_handler, GG_CoapRequestHandler));

    // Init BLE
    gg_esp32_ble_init(gg_loop);

    // Run the loop
    GG_LOG_INFO("--- Running GG Loop ---");
    result = GG_Loop_Run(gg_loop);
    GG_LOG_INFO("--- GG Loop terminated with result %d ---", result);
}

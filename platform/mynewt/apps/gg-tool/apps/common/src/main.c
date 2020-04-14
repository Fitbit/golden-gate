/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include "bsp/bsp.h"
#include "console/console.h"
#include "hal/hal_bsp.h"
#include "hal/hal_gpio.h"
#include "hal/hal_system.h"
#include "nimble/ble.h"
#include "os/os.h"
#include "os/os_sem.h"
#include "shell/shell.h"
#include "sysinit/sysinit.h"

#include "xp/coap/gg_coap.h"
#include "xp/coap/gg_coap_blockwise.h"
#include "xp/coap/handlers/gg_coap_helloworld_handler.h"
#include "xp/common/gg_common.h"
#include "xp/gattlink/gg_gattlink_generic_client.h"
#include "xp/loop/gg_loop.h"
#include "xp/module/gg_module.h"
#include "xp/stack_builder/gg_stack_builder.h"
#include "xp/tls/gg_tls.h"
#include "xp/utils/gg_perf_data_sink.h"
#include "xp/services/blast/gg_blast_service.h"
#include "xp/services/stack/gg_stack_service.h"

#include "nvm.h"

#include "gg_connmgr.h"

// CoAP Client support only on nRF52840
#if NRF52840_XXAA
#include "coap_client.h"
#endif

/*----------------------------------------------------------------------
|   logging
+---------------------------------------------------------------------*/
GG_SET_LOCAL_LOGGER("mynewt.gg-tool");

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/
#define GG_DIAGNOSTICS_RAM_STORAGE_SIZE     (512)
#define GG_PROTOBUF_ENCODE_BUFFER_SIZE      (256)

#define GG_BLAST_DEFAULT_PACKET_SIZE        (30)
#define GG_GATTLINK_RX_WINDOW_SIZE          (4) // 4 eliminates memory allocation failures
#define GG_GATTLINK_TX_WINDOW_SIZE          (4) // 4 eliminates memory allocation failures
#define GG_STACK_MAX_ELEMENTS               (4)

#ifdef NRF52840_XXAA
#define GG_LOOP_TASK_STACK_SIZE             (OS_STACK_ALIGN(2048))
#else
#define GG_LOOP_TASK_STACK_SIZE             (OS_STACK_ALIGN(1024))
#endif
#define GG_LOOP_TASK_PRIORITY               (0x0F)

#define GG_STACK_DTLS_KEY_SIZE              (16)

#define BLE_ATT_MAX_HEADER_SIZE             (5)

#define RESET_CMD_STRING                    "reset"
#define SYS_REBOOT_METHOD                   "device/sys/reboot"
#define SYS_UPTIME_METHOD                   "device/sys/uptime"

#define GG_CMD_STRING                       "gg"
#define GG_AUTO_CONNECT_CMD_STRING          "bt/autoconnect"
#define GG_CONN_PARAMS_CMD_STRING           "bt/set_connection_parameters"
#define GG_COAP_SYNC_DUMP_CMD_STRING        "coap/sync/dump"
#define GG_COAP_SYNC_RESPONSE_CMD_STRING    "coap/sync/response"
#define GG_COAP_HELLOWORLD_CMD_STRING       "coap/helloworld"
#define GG_COAP_CLIENT_CMD_STRING           "coap/client"

#define BT_HANDLER_CONNECT                  "bt/connect"
#define BT_HANDLER_DISCONNECT               "bt/disconnect"
#define BT_HANDLER_MTU_EXCHANGE             "bt/exchange_mtu"
#define BT_HANDLER_SET_ADV_STATE            "bt/enable_advertising"
#define BT_HANDLER_SET_ADV_NAME             "bt/set_advertised_name"
#define BT_HANDLER_GET_CONN_CONFIG          "bt/connection_service/get_connection_configuration"
#define BT_HANDLER_GET_CONN_SVC_STATUS      "bt/connection_service/get_connection_service_status"
#define BT_HANDLER_CONFIG_CONN_SPEEDS       "bt/connection_service/configure_connection_speeds"
#define BT_HANDLER_SET_CONN_SPEED           "bt/connection_service/set_connection_speed"

#define GG_SET_LOG_CONFIG_CLI_STR           "set_log_config"
#define GG_GET_LOG_CONFIG_CLI_STR           "get_log_config"

#define GG_SET_LOG_CONFIG                   "gg/set_log_config"
#define GG_GET_LOG_CONFIG                   "gg/get_log_config"

#define INFO_HANDLER_GET_HOST               "gg/get_host"
#define INFO_HANDLER_GET_PLATFORM           "gg/get_platform"
#define INFO_HANDLER_GET_VERSION            "gg/get_version"

#define PAIR_HANDLER_START_PAIRING          "pair/start_pairing"
#define PAIR_HANDLER_GET_STATE              "pair/get_state"
#define PAIR_HANDLER_GET_PAIRED_DEVICES     "pair/get_paired_devices"

/*
 * These are here if you want to build for a board that is not the nRF52dk.
 * Since these GPIO pins may not be connected to anything, the behaviour
 * may be unexpected, however the app will still compile and run.
 */
#ifndef BUTTON_1
#define BUTTON_1    (13)
#endif

#ifndef BUTTON_2
#define BUTTON_2    (14)
#endif

#ifndef LED_1
#define LED_1       (17)
#endif

#ifndef LED_2
#define LED_2       (18)
#endif

/*----------------------------------------------------------------------
|   forward declarations
+---------------------------------------------------------------------*/
static int StackSetup_Handle(void *arg);
static GG_Result gg_coap_init(void);

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/

typedef struct {
    const char *name;
    shell_cmd_func_t cmd;
} ShellCmdTableEntry;

typedef struct {
    GG_IMPLEMENTS(GG_CoapRequestHandler);
} InternalServerErrorHandler;

typedef struct {
    GG_IMPLEMENTS(GG_CoapResponseListener);

    GG_Timestamp send_time;
} HelloRequester;

typedef struct Psk Psk;
struct Psk {
    const uint8_t* identity;
    size_t         identity_size;
    uint8_t        key[GG_STACK_DTLS_KEY_SIZE];
    Psk*           next;
};

typedef struct {
    GG_IMPLEMENTS(GG_TlsKeyResolver);
    Psk* psks;
} StaticPskResolver;

typedef struct {
    GG_IMPLEMENTS(GG_EventListener);
} StackEventListener;

typedef struct {
    GG_IMPLEMENTS(GG_LoopMessage);

    size_t pkt_size;
    bool start;
} BlasterCmdMessage;

/*----------------------------------------------------------------------
|   globals
+---------------------------------------------------------------------*/
static struct shell_cmd reset_cmd;
static struct shell_cmd gg_cmd;

static char *speed_str[] = {"fast", "slow"};

static os_stack_t g_loop_task_stack[GG_LOOP_TASK_STACK_SIZE];
static struct os_task g_loop_task;

static GG_Loop *g_loop;

static GG_Stack *gg_stack;
static GG_StackBuilderParameters stack_params[GG_STACK_MAX_ELEMENTS];
static StackEventListener stack_event_listener;
static GG_DataSink *user_data_sink;
static GG_DataSource *user_data_source;

#if MYNEWT_VAL(GG_CONNMGR_CENTRAL)
static StaticPskResolver psk_resolver;
#endif

#ifdef NRF52840_XXAA
static HelloRequester hello_requester;
#endif

static GG_StackService *stack_service;
static GG_BlastService *blaster;
static GG_CoapEndpoint *coap_endpoint;

static bool is_stack_ready = false;

static uint8_t BootstrapKeyIdentity[9] = {
        'B', 'O', 'O', 'T', 'S', 'T', 'R', 'A', 'P'
};
static Psk BootstrapPsk = {
    .identity = BootstrapKeyIdentity,
    .identity_size = sizeof(BootstrapKeyIdentity),
    .key = { 0x81, 0x06, 0x54, 0xE3, 0x36, 0xAD, 0xCA, 0xB0,
                0xA0, 0x3C, 0x60, 0xF7, 0x4A, 0xA0, 0xB6, 0xFB }
};

#ifdef NRF52840_XXAA
/*----------------------------------------------------------------------
|   CoAP Request Sender function
+---------------------------------------------------------------------*/
static void HelloRequester_Handler(void *arg)
{
    // Send a request
    GG_CoapRequestHandle request_handle = 0;

    // Setup the request options
    GG_CoapMessageOptionParam options[1] = {
        GG_COAP_MESSAGE_OPTION_PARAM_STRING(URI_PATH, "helloworld")
    };

    // Send a request
    hello_requester.send_time = GG_System_GetCurrentTimestamp();
    GG_CoapEndpoint_SendRequest(coap_endpoint,
                                GG_COAP_METHOD_GET,
                                options,
                                GG_ARRAY_SIZE(options),
                                NULL,
                                0,
                                NULL,
                                GG_CAST(&hello_requester, GG_CoapResponseListener),
                                &request_handle);
}

static void HelloRequester_OnAck(GG_CoapResponseListener* _self)
{
    GG_LOG_INFO("Received CoAP ACK");
}

static void HelloRequester_OnError(GG_CoapResponseListener* _self,
                                   GG_Result error, const char* message)
{
    GG_LOG_INFO("CoAP error: error=%d, message=%s", error, message ? message : "");
}

static void HelloRequester_OnResponse(GG_CoapResponseListener* _self,
                                      GG_CoapMessage* response)
{
    GG_Timestamp recv_time = GG_System_GetCurrentTimestamp();
    unsigned int code = GG_CoapMessage_GetCode(response);
    const char* type_str = "";
    GG_TimeInterval rtt_ms;

    rtt_ms = (recv_time - hello_requester.send_time) / GG_NANOSECONDS_PER_MILLISECOND;

    GG_LOG_INFO("Received CoAP response:");
    GG_LOG_INFO("  rtt = %u.%03u s",
                (int)(rtt_ms / GG_MILLISECONDS_PER_SECOND),
                (int)(rtt_ms % GG_MILLISECONDS_PER_SECOND));
    GG_LOG_INFO("  code = %d.%02d", GG_COAP_MESSAGE_CODE_CLASS(code),
                GG_COAP_MESSAGE_CODE_DETAIL(code));

    switch (GG_CoapMessage_GetType(response)) {
        case GG_COAP_MESSAGE_TYPE_CON:
            type_str = "CON";
            break;

        case GG_COAP_MESSAGE_TYPE_NON:
            type_str = "NON";
            break;

        case GG_COAP_MESSAGE_TYPE_ACK:
            type_str = "ACK";
            break;

        case GG_COAP_MESSAGE_TYPE_RST:
            type_str = "RST";
            break;
    }

    GG_LOG_INFO("  type = %s", type_str);

    uint8_t token[GG_COAP_MESSGAGE_MAX_TOKEN_LENGTH];
    unsigned int token_length = GG_CoapMessage_GetToken(response, token);
    char token_hex[2*GG_COAP_MESSGAGE_MAX_TOKEN_LENGTH+1];

    GG_BytesToHex(token, token_length, token_hex, true);
    token_hex[2*token_length] = 0;
    GG_LOG_INFO("  token = %s", token_hex);

    unsigned int   payload_size = GG_CoapMessage_GetPayloadSize(response);
    const uint8_t* payload      = GG_CoapMessage_GetPayload(response);

    GG_LOG_INFO("  payload size = %d", payload_size);
    GG_LOG_INFO("  payload:");

    for (unsigned int i = 0; i < (payload_size + 15)/ 16; i++) {
        char hex_buffer[33];
        char str_buffer[17];
        unsigned int chunk = 16;

        if (i * 16 + chunk > payload_size) {
            chunk = payload_size - (i * 16);
        }

        GG_BytesToHex(&payload[i*16], chunk, hex_buffer, true);
        memset(str_buffer, ' ', 16);

        for (unsigned int j = 0; j < chunk; j++) {
            uint8_t c = payload[i * 16 + j];

            if (c >= 0x20 && c <= 0x7E) {
                str_buffer[j] = (char)c;
            } else {
                str_buffer[j] = '.';
            }
        }

        str_buffer[16]      = '\0';
        hex_buffer[2*chunk] = '\0';
        GG_LOG_INFO("    %s %s", str_buffer, hex_buffer);
    }
}

/*----------------------------------------------------------------------
|   CoAP Request Sender function table
+---------------------------------------------------------------------*/
GG_IMPLEMENT_INTERFACE(HelloRequester, GG_CoapResponseListener) {
    HelloRequester_OnAck,
    HelloRequester_OnError,
    HelloRequester_OnResponse
};

static int gg_coap_helloworld_cmd_func(int argc, char** argv)
{
    GG_Result res = GG_Loop_InvokeAsync(g_loop, HelloRequester_Handler, NULL);

    if (res != GG_SUCCESS) {
        GG_LOG_WARNING("Async function returned %d", res);
        return 1;
    }

    return 0;
}
#endif

/*----------------------------------------------------------------------
|   TLS definitions
+---------------------------------------------------------------------*/
#if MYNEWT_VAL(GG_CONNMGR_CENTRAL)
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
    if (*key_size < GG_STACK_DTLS_KEY_SIZE) {
        *key_size = GG_STACK_DTLS_KEY_SIZE;
        return GG_ERROR_NOT_ENOUGH_SPACE;
    }

    // look for a match in the list
    Psk* psk = self->psks;
    while (psk) {
        if (key_identity_size == psk->identity_size &&
            !memcmp(key_identity, psk->identity, key_identity_size)) {
            // match! copy the key
            memcpy(key, psk->key, GG_STACK_DTLS_KEY_SIZE);
            *key_size = GG_STACK_DTLS_KEY_SIZE;
            return GG_SUCCESS;
        }
        psk = psk->next;
    }
    return GG_ERROR_NO_SUCH_ITEM;
}

GG_IMPLEMENT_INTERFACE(StaticPskResolver, GG_TlsKeyResolver) {
    StaticPskResolver_ResolvePsk
};

#endif

/*----------------------------------------------------------------------
|   Stack Event Listener interface
+---------------------------------------------------------------------*/
static void StackEventListener_OnEvent(GG_EventListener* self, const GG_Event* event)
{
    const char *stack_type;
    const char *service_type;

    if (event->type != GG_EVENT_TYPE_STACK_EVENT_FORWARD) {
        GG_LOG_WARNING("Unexpected event received!");
        return;
    }

    event = ((GG_StackForwardEvent *)event)->forwarded;

    stack_type = GG_StackService_GetStackType(stack_service);
    service_type = GG_StackService_GetServiceType(stack_service);

    // Check that stack started
    if (!strcmp(stack_type, "dtls")) {
        // DTLS
        if (event->type != GG_EVENT_TYPE_TLS_STATE_CHANGE) {
            return;
        }

        GG_DtlsProtocolStatus dtls_status;
        GG_DtlsProtocol* dtls_protocol = (GG_DtlsProtocol *)(event->source);
        GG_DtlsProtocol_GetStatus(dtls_protocol, &dtls_status);

        if (dtls_status.state != GG_TLS_STATE_SESSION) {
            return;
        }
    } else {
        // Gattlink or UDP
        if (event->type != GG_EVENT_TYPE_GATTLINK_SESSION_READY) {
            return;
        }
    }

    is_stack_ready = true;

    if (!strcmp(service_type, "blast")) {
        // Attach blaster
        GG_BlastService_Attach(blaster, user_data_source, user_data_sink);
        GG_LOG_INFO("To better see perf data run "
                    "'gg log-config plist:.level=INFO' "
                    "to reduce log level");
    } else {
        // Attach CoAP endpoint
        GG_DataSource_SetDataSink(GG_CoapEndpoint_AsDataSource(coap_endpoint),
                                  user_data_sink);
        GG_DataSource_SetDataSink(user_data_source,
                                  GG_CoapEndpoint_AsDataSink(coap_endpoint));
    }

    GG_LOG_INFO("Stack setup complete.");
}

GG_IMPLEMENT_INTERFACE(StackEventListener, GG_EventListener) {
    StackEventListener_OnEvent,
};

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/
static int StackCleanup_Handle(void *arg)
{
    GG_COMPILER_UNUSED(arg);

    // Reset state
    is_stack_ready = false;

    // Cleanup allocated objects
    if (coap_endpoint) {
        GG_DataSource_SetDataSink(GG_CoapEndpoint_AsDataSource(coap_endpoint), NULL);
    }

    if (blaster) {
        GG_BlastService_Attach(blaster, NULL, NULL);
    }

    GG_Stack_Destroy(gg_stack);
    gg_stack = NULL;

    GG_DataSource_SetDataSink(GG_ConnMgr_AsDataSource(), NULL);

    return GG_SUCCESS;
}

static int gg_do_stack_cleanup_sync()
{
    GG_Result result = 0;
    GG_Result res = GG_Loop_InvokeSync(g_loop, StackCleanup_Handle, NULL, &result);

    if (!GG_SUCCEEDED(res) || !GG_SUCCEEDED(result)) {
        GG_LOG_WARNING("Sync function returned %d; Handler function returned %d", res, result);
    }

    return res != GG_SUCCESS ? res : result;
}

static int StackSetup_Handle(void *arg)
{
    static GG_StackElementGattlinkParameters gattlink_param;
    const char *stack_type;
    const char *service_type;
    char *stack_descriptor;
    GG_StackRole stack_role;
    int stack_param_count;
    GG_Result rc;

    GG_COMPILER_UNUSED(arg);

    // automatically cleanup any previous state
    if (gg_stack) {
        StackCleanup_Handle(NULL);
    }

    stack_type = GG_StackService_GetStackType(stack_service);
    service_type = GG_StackService_GetServiceType(stack_service);

    GG_LOG_INFO("Stack setup: stack_type=%s service_type=%s", stack_type, service_type);

#if MYNEWT_VAL(GG_CONNMGR_PERIPHERAL)
    stack_role = GG_STACK_ROLE_NODE;
#else
    stack_role = GG_STACK_ROLE_HUB;
#endif

    stack_param_count = 0;

    // Configure GG Stack
    if (!strcmp(stack_type, "gattlink")) {
        stack_descriptor = GG_STACK_DESCRIPTOR_GATTLINK_ONLY;
    } else if (!strcmp(stack_type, "udp")) {
        stack_descriptor = GG_STACK_DESCRIPTOR_SOCKET_NETIF_GATTLINK;
    } else { // dtls
        stack_descriptor = GG_STACK_DESCRIPTOR_DTLS_SOCKET_NETIF_GATTLINK;
    }

    gattlink_param.rx_window = GG_GATTLINK_RX_WINDOW_SIZE;
    gattlink_param.tx_window = GG_GATTLINK_TX_WINDOW_SIZE;
    gattlink_param.buffer_size = 0;
    gattlink_param.initial_max_fragment_size = GG_ConnMgr_GetMTUSize() - BLE_ATT_MAX_HEADER_SIZE;

    stack_params[stack_param_count].element_type = GG_STACK_ELEMENT_TYPE_GATTLINK;
    stack_params[stack_param_count].element_parameters = &gattlink_param;
    stack_param_count++;

    // Initialize a key resolver if needed
    if (!strcmp(stack_type, "dtls")) {
        static uint16_t cipher_suites[3] = {
            GG_TLS_PSK_WITH_AES_128_CCM,
            GG_TLS_PSK_WITH_AES_128_GCM_SHA256,
            GG_TLS_ECDHE_PSK_WITH_AES_128_CBC_SHA256
        };

#if MYNEWT_VAL(GG_CONNMGR_CENTRAL)
        Psk* psks = &BootstrapPsk;

        GG_SET_INTERFACE(&psk_resolver, StaticPskResolver, GG_TlsKeyResolver);
        psk_resolver.psks = psks;

        static GG_TlsServerOptions dtls_server_parameters = {
            .base = {
                .cipher_suites       = cipher_suites,
                .cipher_suites_count = GG_ARRAY_SIZE(cipher_suites)
            },
            .key_resolver = GG_CAST(&psk_resolver, GG_TlsKeyResolver)
        };

        stack_params[stack_param_count].element_type = GG_STACK_ELEMENT_TYPE_DTLS_SERVER;
        stack_params[stack_param_count].element_parameters = &dtls_server_parameters;
        stack_param_count++;
#else
        static GG_TlsClientOptions tls_options = {
            .base = {
                .cipher_suites       = cipher_suites,
                .cipher_suites_count = GG_ARRAY_SIZE(cipher_suites)
            },
            .psk_identity      = BootstrapKeyIdentity,
            .psk_identity_size = sizeof(BootstrapKeyIdentity),
            .psk               = BootstrapPsk.key,
            .psk_size          = sizeof(BootstrapPsk.key)
        };

        stack_params[stack_param_count].element_type = GG_STACK_ELEMENT_TYPE_DTLS_CLIENT;
        stack_params[stack_param_count].element_parameters = &tls_options;
        stack_param_count++;
#endif
    }

    // Build GG Stack
    rc = GG_StackBuilder_BuildStack(stack_descriptor,
                                    stack_params,
                                    stack_param_count,
                                    stack_role,
                                    NULL,
                                    g_loop,
                                    GG_ConnMgr_AsDataSource(),
                                    GG_ConnMgr_AsDataSink(),
                                    &gg_stack);
    if (rc != GG_SUCCESS) {
        GG_LOG_WARNING("Failed to build GG stack!");
        goto stack_setup_exit;
    }

    // Get top Data Sink and Source
    GG_StackElementPortInfo stack_elem;
    rc = GG_Stack_GetPortById(gg_stack, GG_STACK_ELEMENT_ID_TOP,
                              GG_STACK_PORT_ID_TOP, &stack_elem);
    if (rc != GG_SUCCESS) {
        GG_LOG_WARNING("Failed to get stack top!");
        goto stack_setup_exit;
    }

    user_data_sink = stack_elem.sink;
    user_data_source = stack_elem.source;

    GG_SET_INTERFACE(&stack_event_listener, StackEventListener, GG_EventListener);
    GG_EventEmitter_SetListener(GG_Stack_AsEventEmitter(gg_stack),
                                GG_CAST(&stack_event_listener, GG_EventListener));

    GG_Stack_Start(gg_stack);

    return GG_SUCCESS;

stack_setup_exit:
    GG_Loop_PostMessage(g_loop, GG_Loop_CreateTerminationMessage(g_loop), 0);

    return GG_FAILURE;
}

static int gg_do_stack_setup_sync(void) {
    GG_Result result = 0;
    GG_Result res = GG_Loop_InvokeSync(g_loop, StackSetup_Handle, NULL, &result);

    if (!GG_SUCCEEDED(res) || !GG_SUCCEEDED(result)) {
        GG_LOG_WARNING("Sync function returned %d; Handler function returned %d",
                       res, result);
    }

    return res != GG_SUCCESS ? res : result;
}

/*----------------------------------------------------------------------
|   gg_loop_task
+---------------------------------------------------------------------*/
static void gg_loop_task(void *arg)
{
    GG_Result rc;

    rc = gg_coap_init();
    if (rc == GG_SUCCESS) {
        GG_Loop_Run(g_loop);
    } else {
        GG_LOG_WARNING("Failed to create coap endpoint!");
    }

    // Mynewt tasks are not allowed to ever terminate
    while (1) {
        os_time_delay(OS_TICKS_PER_SEC);
    }
}

/*----------------------------------------------------------------------
|   gg_on_connected
+---------------------------------------------------------------------*/
static void gg_on_connected(GG_Result status)
{
    if (status != GG_SUCCESS) {
        return;
    }

    GG_LOG_INFO("conn_mgr: peer connected");

    status = gg_do_stack_setup_sync();
    if (status != GG_SUCCESS) {
        gg_do_stack_cleanup_sync();
    }
}

/*----------------------------------------------------------------------
|   gg_on_disconnected
+---------------------------------------------------------------------*/
static void gg_on_disconnected(void)
{
    GG_LOG_INFO("conn_mgr: peer disconnected");

    gg_do_stack_cleanup_sync();
}

/*----------------------------------------------------------------------
|   MtuChange_Handle
+---------------------------------------------------------------------*/
static int MtuChange_Handle(void *mtu_ptr)
{
    GG_StackLinkMtuChangeEvent event;

    if (mtu_ptr == NULL) {
        return GG_ERROR_INVALID_STATE;
    }

    if (gg_stack == NULL) {
        return GG_SUCCESS;
    }

    event.base.type = GG_EVENT_TYPE_LINK_MTU_CHANGE;
    event.base.source = NULL;
    event.link_mtu = *((uint16_t *)mtu_ptr) - BLE_ATT_MAX_HEADER_SIZE;

    GG_EventListener_OnEvent(GG_Stack_AsEventListener(gg_stack), &event.base);

    return GG_SUCCESS;
}

/*----------------------------------------------------------------------
|   gg_on_mtu_size_change
+---------------------------------------------------------------------*/
static void gg_on_mtu_size_change(uint16_t size)
{
    int result = 0;
    GG_Result rc;

    GG_LOG_INFO("conn_mgr: mtu size changed to %d", size);

    if (gg_stack == NULL) {
        return;
    }

    rc = GG_Loop_InvokeSync(g_loop, MtuChange_Handle, &size, &result);

    if (rc != GG_SUCCESS || result != GG_SUCCESS) {
        GG_LOG_WARNING("Loop InvokeSync returned %d; MTU change handler returned %d",
                       rc, result);
    }
}

/*----------------------------------------------------------------------
|   gg_connmgr_init
+---------------------------------------------------------------------*/
static void gg_connmgr_init(void)
{
    GG_ConnMgr_Client_Callback_Functions cbs;
    GG_Result ret;

    cbs.connected = gg_on_connected;
    cbs.disconnected = gg_on_disconnected;
    cbs.mtu_size_change = gg_on_mtu_size_change;

    ret = GG_ConnMgr_Initialize(g_loop);
    GG_ASSERT(ret == GG_SUCCESS);

    GG_ConnMgr_RegisterClient(&cbs);
}

/*----------------------------------------------------------------------
|   gg_coap_init
+---------------------------------------------------------------------*/
static GG_Result gg_coap_init(void)
{
    GG_Result rc;

    // Setup CoAP endpoint
    rc = GG_CoapEndpoint_Create(GG_Loop_GetTimerScheduler(g_loop),
                                NULL,
                                NULL,
                                &coap_endpoint);
    if (rc != GG_SUCCESS) {
        return rc;
    }

    GG_CoapEndpoint_Register_HelloworldHandler(coap_endpoint,
                                               GG_COAP_REQUEST_HANDLER_FLAG_ALLOW_GET |
                                               GG_COAP_REQUEST_HANDLER_FLAG_ALLOW_PUT);
#ifdef NRF52840_XXAA
    GG_SET_INTERFACE(&hello_requester, HelloRequester, GG_CoapResponseListener);
#endif

#ifdef NRF52840_XXAA
    rc = CoapClient_Init(g_loop, coap_endpoint);
    if (rc != GG_SUCCESS) {
        return rc;
    }
#endif

    return GG_SUCCESS;
}

/*----------------------------------------------------------------------
|   reset_cmd_func
+---------------------------------------------------------------------*/
static int reset_cmd_func(int argc, char **argv)
{
    if (argc != 1) {
        goto reset_cmd_usage;
    }

    hal_system_reset();

    return 0;

reset_cmd_usage:
    console_printf("Usage:\n  %s\n", argv[0]);
    return 1;
}

/*----------------------------------------------------------------------
|   gg_stack_cmd_func
+---------------------------------------------------------------------*/
static int gg_stack_cmd_func(int argc, char **argv)
{
    GG_Result rc;

    if (argc != 2 && argc != 3) {
        goto gg_stack_cmd_usage;
    }

    if (argc == 2) {
        rc = GG_StackService_SetType(stack_service, argv[1], NULL);
    } else {
        rc = GG_StackService_SetType(stack_service, argv[1], argv[2]);
    }

    if (rc != GG_SUCCESS) {
        goto gg_stack_cmd_usage;
    }

    return 0;

gg_stack_cmd_usage:
    console_printf("Usage:\n"
                   "  gg %s <gattlink> [<blast>]\n"
                   "  gg %s <udp|dtls> [<coap|blast>]\n",
                   argv[0], argv[0]);
    return 1;
}

/*----------------------------------------------------------------------
|   gg_blast_cmd_func
+---------------------------------------------------------------------*/
static int gg_blast_cmd_func(int argc, char **argv)
{
    GG_PerfDataSinkStats stats;
    size_t pkt_size = GG_BLAST_DEFAULT_PACKET_SIZE;
    size_t pkt_count = 0;
    size_t pkt_interval = 0;

    const char *service_type = GG_StackService_GetServiceType(stack_service);

    if (strcmp(service_type, "blast")) {
        console_printf("Need to set stack service type to 'blast'!\n");
        return 1;
    }

    if (argc != 1 && argc != 2 && argc != 4) {
        goto gg_blast_cmd_usage;
    }

    if (!is_stack_ready) {
        console_printf("Blaster not yet initialized!\n");
        return 1;
    }

    if (!strcmp(argv[0], GG_BLAST_SERVICE_START_METHOD) && (argc == 2 || argc == 4)) {
        pkt_size = atoi(argv[1]);

        if (argc == 4) {
            pkt_count = atoi(argv[2]);
            pkt_interval = atoi(argv[3]);
        }

        GG_BlastService_StartBlaster(blaster, pkt_size, pkt_count, pkt_interval);
    } else if (argc != 1) {
        goto gg_blast_cmd_usage;
    } else if (!strcmp(argv[0], GG_BLAST_SERVICE_STOP_METHOD)) {
        GG_BlastService_StopBlaster(blaster);
        return 0;
    } else if (!strcmp(argv[0], GG_BLAST_SERVICE_RESET_STATS_METHOD)) {
        GG_BlastService_ResetStats(blaster);
    } else if (!strcmp(argv[0], GG_BLAST_SERVICE_GET_STATS_METHOD)) {
        GG_BlastService_GetStats(blaster, &stats);

        console_printf("%u.%02u kBps - %u packets - %u bytes - %u gaps\n",
                       (int)(stats.throughput / 1000.0),
                       ((int)(stats.throughput / 10.0)) % 100,
                       (int)stats.packets_received,
                       (int)stats.bytes_received,
                       (int)stats.gap_count);
    } else {
        goto gg_blast_cmd_usage;
    }

    return 0;

gg_blast_cmd_usage:
    console_printf("Usage:\n"
                   "  gg %s <pkt_size> [<pkt_count> <pkt_interval>]\n"
                   "  gg %s\n"
                   "  gg %s\n"
                   "  gg %s\n",
                   GG_BLAST_SERVICE_START_METHOD,
                   GG_BLAST_SERVICE_STOP_METHOD,
                   GG_BLAST_SERVICE_GET_STATS_METHOD,
                   GG_BLAST_SERVICE_RESET_STATS_METHOD);
    return 1;
}


/*----------------------------------------------------------------------
|   gg_set_log_config_cmd_func
+---------------------------------------------------------------------*/
static int gg_set_log_config_cmd_func(int argc, char **argv)
{
    nvm_error_t rc;

    if ((argc == 2) && !strcmp(argv[0], GG_SET_LOG_CONFIG_CLI_STR)) {
        rc = nvm_set_log_config(argv[1]);
    } else {
        console_printf("Usage:\n  gg %s <config_string>\n", argv[0]);
        return 1;
    }

    if (rc == NVM_OK) {
        console_printf("Log config saved to NVM.\n");

        // reconfigure logging now
        GG_LogManager_Configure(NULL);
    } else {
        console_printf("Failed to save log config to NVM!\n");
    }

    return 0;
}


/*----------------------------------------------------------------------
|   gg_get_log_config_cmd_func
+---------------------------------------------------------------------*/
static int gg_get_log_config_cmd_func(int argc, char **argv)
{
    char buf[LOG_CONFIG_MAX_LEN + 1];
    nvm_error_t rc;

    if ((argc == 1) && !strcmp(argv[0], GG_GET_LOG_CONFIG_CLI_STR)) {
        memset(buf, 0, sizeof(buf));
        rc = nvm_get_log_config(buf, LOG_CONFIG_MAX_LEN);

        if (rc == NVM_OK) {
            console_printf("Stored log config string:\n%s\n", buf);
        } else if (rc == NVM_NOT_SET) {
            console_printf("Log string not set\n");
            return 1;
        } else {
            console_printf("Get log string failed: %d\n", rc);
            return 1;
        }
    } else {
        console_printf("Usage:\n  gg %s\n", argv[0]);
        return 1;
    }

    return 0;
}


/*----------------------------------------------------------------------
|   gg_disconnect_cmd_func
+---------------------------------------------------------------------*/
static int gg_disconnect_cmd_func(int argc, char **argv)
{
    if (argc != 1) {
        goto gg_disconnect_cmd_usage;
    }

    GG_ConnMgr_Disconnect();

    return 0;

gg_disconnect_cmd_usage:
    console_printf("Usage:\n  gg %s\n", argv[0]);
    return 1;
}


/*----------------------------------------------------------------------
|   gg_adv_set_state_cmd_func - peripheral only
+---------------------------------------------------------------------*/
#if MYNEWT_VAL(GG_CONNMGR_PERIPHERAL)
static int gg_adv_set_state_cmd_func(int argc, char **argv)
{
    GG_Result rc;

    if (argc != 2) {
        goto gg_adv_set_state_cmd_usage;
    }

    if (!strcmp(argv[1], "on")) {
        rc = GG_ConnMgr_AdvertiseEnable();
    } else if (!strcmp(argv[1], "off")) {
        rc = GG_ConnMgr_AdvertiseDisable();
    } else {
        goto gg_adv_set_state_cmd_usage;
    }

    if (rc == GG_SUCCESS) {
        console_printf("Success\n");
    } else {
        console_printf("Returned error: %d\n", rc);
    }

    return 0;

gg_adv_set_state_cmd_usage:
    console_printf("Usage:\n  gg %s on/off\n", argv[0]);
    return 1;
}
#endif

/*----------------------------------------------------------------------
|   gg_mtu_update_cmd_func
+---------------------------------------------------------------------*/
static int gg_mtu_update_cmd_func(int argc, char **argv)
{
    uint16_t mtu;
    GG_Result rc;

    if (argc != 2) {
        goto gg_mtu_update_cmd_usage;
    }

    mtu = atoi(argv[1]);

    rc = GG_ConnMgr_ChangeMTUSize(mtu);
    if (rc == GG_ERROR_INVALID_STATE) {
        console_printf("MTU exchange can be done only once by Mynewt OS! "
                       "Run the 'gg bt/exchange_mtu' command before establishing "
                       "a connection for the update to work!\n");
        return 1;
    } else if (rc != GG_SUCCESS) {
        console_printf("MTU change request failed!\n");
        return 1;
    }

    return 0;

gg_mtu_update_cmd_usage:
    console_printf("Usage:\n  gg %s <mtu>\n", argv[0]);
    return 1;
}

/*----------------------------------------------------------------------
|   gg_conn_params_cmd_func
+---------------------------------------------------------------------*/
static int gg_conn_params_cmd_func(int argc, char **argv)
{
    GG_LinkConfiguration_ConnectionModeConfig config;
    GG_Result rc;

    if (argc != 5) {
        goto gg_conn_params_cmd_usage;
    }

    if (GG_ConnMgr_GetState() == GG_CONNECTION_MANAGER_STATE_DISCONNECTED) {
        console_printf("Can only change Conn Params if a connection is established!\n");
        return 1;
    }

    config.min_connection_interval = atoi(argv[1]);
    config.max_connection_interval = atoi(argv[2]);
    config.slave_latency           = atoi(argv[3]);
    config.supervision_timeout     = atoi(argv[4]) / 10; // convert from 10ms units to 100ms units

    rc = GG_ConnMgr_ChangeConnectionConfig(&config);
    if (rc != GG_SUCCESS) {
        console_printf("Conn Params change request failed!\n");
        return 1;
    }

    return 0;

gg_conn_params_cmd_usage:
    console_printf("Usage:\n"
                   "  gg %s <min_conn_interval_1p25ms> "
                   "<max_conn_interval_1p25ms> <slave_latency> "
                   "<supervision_timeout_10ms>\n",
                   argv[0]);
    return 1;
}

/*----------------------------------------------------------------------
|   gg_get_conn_svc_status_cmd_func
+---------------------------------------------------------------------*/
static int gg_get_conn_svc_status_cmd_func(int argc, char **argv)
{
    if (argc != 1) {
        goto gg_get_conn_svc_status_cmd_usage;
    }

    GG_ConnMgrState gg_con_state = GG_ConnMgr_GetState();
    bool ble_connected = gg_con_state != GG_CONNECTION_MANAGER_STATE_DISCONNECTED;
    bool link_up = gg_con_state == GG_CONNECTION_MANAGER_STATE_CONNECTED;

    GG_LinkStatus_ConnectionStatus* status = GG_ConnMgr_GetConnStatus();
    bool bonded    = (status->flags & GG_LINK_STATUS_CONNECTION_STATUS_FLAG_HAS_BEEN_BONDED_BEFORE) != 0;
    bool encrypted = (status->flags & GG_LINK_STATUS_CONNECTION_STATUS_FLAG_ENCRYPTED) != 0;

    console_printf("Connection Service STATUS:\n"
                   "Connected?:              %d\n",
                   ble_connected);

    if (ble_connected) {
        console_printf("Bonded?:    %d\n"
                       "Encrypted?: %d\n"
                       "Connected?: %d\n"
                       "Link Up?:   %d\n",
                       bonded,
                       encrypted,
                       ble_connected,
                       link_up);
    }

    return 0;

gg_get_conn_svc_status_cmd_usage:
    console_printf("Usage:\n  gg %s\n", argv[0]);
    return 1;
}

/*----------------------------------------------------------------------
|   gg_get_conn_config_cmd_func
+---------------------------------------------------------------------*/
static int gg_get_conn_config_cmd_func(int argc, char **argv)
{
    if (argc != 1) {
        goto gg_get_conn_config_cmd_usage;
    }

    GG_ConnMgrState gg_con_state = GG_ConnMgr_GetState();
    bool ble_connected = gg_con_state != GG_CONNECTION_MANAGER_STATE_DISCONNECTED;

    console_printf("Connection Service CONFIG:\n"
                   "Connected?:          %d\n", ble_connected);

    if (ble_connected) {
        const GG_LinkStatus_ConnectionConfig* config = GG_ConnMgr_GetConnConfig();

        console_printf("Connection Interval: %d\n"
                       "Slave Latency:       %d\n"
                       "Supervision Timeout: %d\n"
                       "MTU:                 %d\n",
                       config->connection_interval,
                       config->slave_latency,
                       config->supervision_timeout,
                       config->mtu);
    }

    return 0;

gg_get_conn_config_cmd_usage:
    console_printf("Usage:\n  gg %s\n", argv[0]);
    return 1;
}

/*----------------------------------------------------------------------
|   gg_set_conn_speed_cmd_func
+---------------------------------------------------------------------*/
static int gg_set_conn_speed_cmd_func(int argc, char **argv)
{
    if (GG_ConnMgr_GetState() != GG_CONNECTION_MANAGER_STATE_CONNECTED) {
        console_printf("No GG connection established yet!\n");
        return 1;
    }

    if (argc != 2) {
        goto gg_set_conn_speed_cmd_usage;
    }

    int speed = -1;
    for (unsigned int i = 0; i < GG_ARRAY_SIZE(speed_str); i++) {
        if (!strcmp(speed_str[i], argv[1])) {
            speed = i;
            break;
        }
    }

    if (speed == -1) {
        goto gg_set_conn_speed_cmd_usage;
    }

    GG_Result rc = GG_ConnMgr_ChangeConnectionSpeed((GG_LinkConfiguration_ConnectionSpeed)speed);
    if (rc != GG_SUCCESS) {
        console_printf("Failed to update Connection Speed!\n");
        return 1;
    }

    return 0;

gg_set_conn_speed_cmd_usage:
    console_printf("Usage:\n  gg %s fast|slow\n", argv[0]);
    return 1;
}

/*----------------------------------------------------------------------
|   gg_config_conn_speeds_cmd_func
+---------------------------------------------------------------------*/
static int gg_config_conn_speeds_cmd_func(int argc, char **argv)
{
    int arg_no = 1 + 4 * GG_ARRAY_SIZE(speed_str);

    if (argc != arg_no) {
        goto gg_config_conn_speeds_cmd_usage;
    }

    GG_LinkConfiguration_ConnectionConfig config;
    config.mask = GG_LINK_CONFIGURATION_CONNECTION_CONFIG_HAS_FAST_MODE_CONFIG |
                  GG_LINK_CONFIGURATION_CONNECTION_CONFIG_HAS_SLOW_MODE_CONFIG;
    for (unsigned int i = 1; i < arg_no; i += 4) {
        GG_LinkConfiguration_ConnectionModeConfig mode_config;
        mode_config.min_connection_interval = atoi(argv[i]);
        mode_config.max_connection_interval = atoi(argv[i + 1]);
        mode_config.slave_latency           = atoi(argv[i + 2]);
        mode_config.supervision_timeout     = atoi(argv[i + 3]) / 10; // convert from 10ms to 100ms units
        if (i == 1) {
            config.fast_mode_config = mode_config;
        } else {
            config.slow_mode_config = mode_config;
        }
    }

    GG_Result rc = GG_ConnMgr_SetPreferredConnectionConfig(&config);
    if (rc != GG_SUCCESS) {
        console_printf("Failed to update Connection Speed configs!\n");
        return 1;
    }

    return 0;

gg_config_conn_speeds_cmd_usage:
    console_printf("Usage:\n  gg %s\n", argv[0]);

    for (unsigned int i = 0; i < GG_ARRAY_SIZE(speed_str); i++) {
        console_printf("    <%s_min_conn_interval_1p25ms> <%s_max_conn_interval_1p25ms> "
                       "<%s_slave_latency> <%s_supervision_timeout_10ms>\n",
                       speed_str[i], speed_str[i], speed_str[i], speed_str[i]);
    }

    return 1;
}

#if MYNEWT_VAL(GG_CONNMGR_CENTRAL)
/*----------------------------------------------------------------------
|   parser_ble_addr
+---------------------------------------------------------------------*/
static int parse_ble_addr(char *str, uint8_t *addr)
{
    int val;
    int i;

    // String format should be XX:XX:XX:XX:XX:XX
    if (strlen(str) != (BLE_DEV_ADDR_LEN * 3 - 1)) {
        return -1;
    }

    for (i = 0; i < (BLE_DEV_ADDR_LEN * 3 - 1); i++) {
        if (i % 3 == 2) {
            if (str[i] != ':') {
                return -1;
            }
        } else {
            if (!isxdigit(str[i])) {
                return -1;
            }
        }
    }

    for (i = 0; i < BLE_DEV_ADDR_LEN; i++) {
        sscanf(str + i * 3, "%2X", &val);
        addr[5 - i] = val;
    }

    return 0;
}

/*----------------------------------------------------------------------
|   gg_connect_cmd_func
+---------------------------------------------------------------------*/
static int gg_connect_cmd_func(int argc, char **argv)
{
    ble_addr_t addr;
    int rc;

    if (GG_ConnMgr_GetState() != GG_CONNECTION_MANAGER_STATE_DISCONNECTED) {
        console_printf("Already connected!\n");
        return 1;
    }

    if (argc == 1) {
        GG_ConnMgr_ScanAndConnect(NULL);
        return 0;
    }

    if (argc != 2 && argc != 3) {
        goto gg_connect_cmd_usage;
    }

    rc = parse_ble_addr(argv[1], addr.val);
    if (rc != 0) {
        // Not a BLE address so assume it's an advertised name
        GG_ConnMgr_ScanAndConnect(argv[1]);
        return 0;
    }

    if (argc == 2) {
        addr.type = BLE_ADDR_RANDOM;
    } else if (!strcmp(argv[2], "random")) {
        addr.type = BLE_ADDR_RANDOM;
    } else if (!strcmp(argv[2], "public")) {
        addr.type = BLE_ADDR_PUBLIC;
    } else {
        goto gg_connect_cmd_usage;
    }

    GG_ConnMgr_Connect(&addr);

    return 0;

gg_connect_cmd_usage:
    console_printf("Usage:\n"
                   "  gg %s\n"
                   "  gg %s <peer_name>\n"
                   "  gg %s XX:XX:XX:XX:XX:XX [random|public]\n",
                   argv[0], argv[0], argv[0]);
    return 1;
}

/*----------------------------------------------------------------------
|   gg_auto_connect_cmd_func
+---------------------------------------------------------------------*/
static int gg_auto_connect_cmd_func(int argc, char **argv)
{
    ble_addr_t addr;
    int rc;

    if (argc > 3) {
        goto gg_auto_connect_cmd_usage;
    }

    if (argc == 1) {
        nvm_set_peer_addr(NULL);
        return 0;
    } else {
        rc = parse_ble_addr(argv[1], addr.val);
        if (rc != 0) {
            goto gg_auto_connect_cmd_usage;
        }
    }

    if (argc == 3) {
        if (!strcmp(argv[2], "random")) {
            addr.type = BLE_ADDR_RANDOM;
        } else if (!strcmp(argv[2], "public")) {
            addr.type = BLE_ADDR_PUBLIC;
        } else {
            goto gg_auto_connect_cmd_usage;
        }
    } else {
        addr.type = BLE_ADDR_RANDOM;
    }

    rc = nvm_set_peer_addr(&addr);
    if (rc != 0) {
        console_printf("Failed to save peer addr to NVM!");
    }

    if (GG_ConnMgr_GetState() == GG_CONNECTION_MANAGER_STATE_DISCONNECTED) {
        GG_ConnMgr_Connect(&addr);
    }

    return 0;

gg_auto_connect_cmd_usage:
    console_printf("Usage:\n"
                   "  gg %s\n"
                   "  gg %s XX:XX:XX:XX:XX:XX [random|public]\n",
                   argv[0], argv[0]);
    return 1;
}

#ifdef NRF52840_XXAA

/*----------------------------------------------------------------------
 |   gg_sync_cmd_func
 +---------------------------------------------------------------------*/
typedef struct {
    GG_IMPLEMENTS(GG_CoapBlockwiseResponseListener);

    size_t                  blocks_received;
    size_t                  bytes_received;
    GG_Result               last_error;
    GG_CoapMessageBlockInfo last_block_info;
} BlockListener;

static BlockListener block_listener = {0};

static void
BlockListener_OnResponseBlock(GG_CoapBlockwiseResponseListener* _self,
                              GG_CoapMessageBlockInfo*          block_info,
                              GG_CoapMessage*                   block_message)
{
    BlockListener* self = GG_SELF(BlockListener, GG_CoapBlockwiseResponseListener);
    GG_CoapMessageBlockInfo block = {0};

    GG_LOG_FINE("#### > Getting block info");
    GG_Result result = GG_CoapMessage_GetBlockInfo(block_message, GG_COAP_MESSAGE_OPTION_BLOCK2,
                                                   &block, GG_COAP_BLOCKWISE_DEFAULT_BLOCK_SIZE);
    if (GG_FAILED(result)) {
        GG_LOG_SEVERE("##### > Failed to gather block info from message");
    }

    self->last_block_info = *block_info;
    if (GG_COAP_MESSAGE_CODE_CLASS(GG_CoapMessage_GetCode(block_message)) ==
        GG_COAP_MESSAGE_CODE_CLASS_SUCCESS_RESPONSE) {
        ++self->blocks_received;
        GG_LOG_FINE("##### > Total blocks received: %d", self->blocks_received);
        self->bytes_received += GG_CoapMessage_GetPayloadSize(block_message);
        GG_LOG_FINE("##### > Total bytes received: %d", self->bytes_received);
    }

    if (block.more) {
        GG_LOG_FINE("##### > More blocks to come");
    } else {
        GG_LOG_FINE("##### > Last block received");
    }

    // Pretty print the payload to verify data
    const uint8_t* payload_ptr = GG_CoapMessage_GetPayload(block_message);
    size_t i = 0;
    size_t payload_size = GG_CoapMessage_GetPayloadSize(block_message);

    while (i < payload_size) {
        for (int j = 0; j < 16 && i < payload_size; ++j) {
            console_printf("%02x ", *payload_ptr);
            payload_ptr++;
            ++i;
        }
        console_printf("\n");
    }
}

static void
BlockListener_OnError(GG_CoapBlockwiseResponseListener* _self,
                      GG_Result                         error,
                      const char*                       message)
{
    GG_LOG_SEVERE("##### > Sync blockwise listener error code: %d, message: %s", error, message);
    BlockListener* self = GG_SELF(BlockListener, GG_CoapBlockwiseResponseListener);
    GG_COMPILER_UNUSED(message);

    self->last_error = error;
}

GG_IMPLEMENT_INTERFACE(BlockListener, GG_CoapBlockwiseResponseListener) {
    .OnResponseBlock = BlockListener_OnResponseBlock,
    .OnError         = BlockListener_OnError
};

static void Sync_Handle(void *arg)
{
    GG_Result result;
    GG_LOG_FINE("##### > Starting sync test");

    GG_ASSERT(coap_endpoint);

    // create a blockwise listener
    memset(&block_listener, 0, sizeof(BlockListener));
    GG_SET_INTERFACE(&block_listener, BlockListener, GG_CoapBlockwiseResponseListener);

    // make a blockwise GET request for sync/dump
    GG_CoapRequestHandle request_handle;
    GG_CoapMessageOptionParam params[2] = {
        GG_COAP_MESSAGE_OPTION_PARAM_STRING(URI_PATH, "sync"),
        GG_COAP_MESSAGE_OPTION_PARAM_STRING(URI_PATH, "dump")
    };

    GG_LOG_FINE("##### > Sending sync block wise request");
    result = GG_CoapEndpoint_SendBlockwiseRequest(coap_endpoint,
                                                  GG_COAP_METHOD_GET,
                                                  params, GG_ARRAY_SIZE(params),
                                                  NULL,
                                                  0,
                                                  NULL,
                                                  GG_CAST(&block_listener, GG_CoapBlockwiseResponseListener),
                                                  &request_handle);
    GG_LOG_FINE("Block wise request rc=%d", result);
}

//-----------------------------------------------------------------------
static int gg_sync_cmd_func(int argc, char** argv)
{
    GG_Result res = GG_Loop_InvokeAsync(g_loop, Sync_Handle, NULL);

    if (res != GG_SUCCESS) {
        GG_LOG_WARNING("Async function returned %d", res);
        return 1;
    }

    return 0;
}

#endif

#ifdef NRF52840_XXAA

//----------------------------------------------------------------------
// CoAP payload source that returns a large payload
//----------------------------------------------------------------------
typedef struct {
    GG_IMPLEMENTS(GG_CoapBlockSource);

    size_t payload_size;
} BlockSource;

static BlockSource block_source = {0};

static GG_Result
BlockSource_GetDataSize(GG_CoapBlockSource* _self,
                        size_t              offset,
                        size_t*             data_size,
                        bool*               more)
{
    BlockSource* self = GG_SELF(BlockSource, GG_CoapBlockSource);
    GG_Result rc;

    rc = GG_CoapMessageBlockInfo_AdjustAndGetChunkSize(
        offset,
        data_size,
        more,
        self->payload_size);

    if (rc != GG_SUCCESS) {
        GG_LOG_WARNING("GG_CoapMessageBlockInfo_AdjustAndGetChunkSize failed!");
        return rc;
    }

    return GG_SUCCESS;
}

static GG_Result
BlockSource_GetData(GG_CoapBlockSource* _self,
                    size_t              offset,
                    size_t              data_size,
                    void*               data)
{
    bool more_blocks;
    GG_Result rc;

    GG_COMPILER_UNUSED(_self);

    rc = BlockSource_GetDataSize(_self, offset, &data_size, &more_blocks);
    if (rc != GG_SUCCESS) {
        return rc;
    }

    // Fill in dummy data for sync response
    uint8_t* payload = (uint8_t*)data;
    for (unsigned int i = 0; i < data_size; i++) {
        payload[i] = 0x01;
    }

    return GG_SUCCESS;
}

GG_IMPLEMENT_INTERFACE(BlockSource, GG_CoapBlockSource) {
    .GetDataSize = BlockSource_GetDataSize,
    .GetData     = BlockSource_GetData
};

//-----------------------------------------------------------------------
static void SyncResponse_Handle(void *arg)
{
    GG_Result result;
    GG_LOG_FINE("##### > Starting sync response test");

    GG_ASSERT(coap_endpoint);

    // Create a block source
    GG_SET_INTERFACE(&block_source, BlockSource, GG_CoapBlockSource);
    block_source.payload_size = GG_COAP_BLOCKWISE_DEFAULT_BLOCK_SIZE;

    GG_CoapRequestHandle request_handle = 0;
    GG_CoapMessageOptionParam params[3] = {
        GG_COAP_MESSAGE_OPTION_PARAM_STRING(URI_PATH, "sync"),
        GG_COAP_MESSAGE_OPTION_PARAM_STRING(URI_PATH, "response"),
        GG_COAP_MESSAGE_OPTION_PARAM_UINT(CONTENT_FORMAT, GG_COAP_MESSAGE_FORMAT_ID_OCTET_STREAM)
    };

    result = GG_CoapEndpoint_SendBlockwiseRequest(coap_endpoint,
                                                  GG_COAP_METHOD_PUT,
                                                  params, GG_ARRAY_SIZE(params),
                                                  GG_CAST(&block_source, GG_CoapBlockSource),
                                                  GG_COAP_BLOCKWISE_DEFAULT_BLOCK_SIZE,
                                                  NULL,
                                                  NULL,
                                                  &request_handle);

    GG_LOG_FINE("Block wise sync response request rc=%d", result);
}

//-----------------------------------------------------------------------
static int gg_sync_response_cmd_func(int argc, char** argv)
{
    GG_Result res = GG_Loop_InvokeAsync(g_loop, SyncResponse_Handle, NULL);

    if (res!= GG_SUCCESS) {
        GG_LOG_WARNING("Async function returned %d", res);
        return 1;
    }

    return 0;
}
#endif

#endif

/*----------------------------------------------------------------------
|   gg_adv_name_cmd_func
+---------------------------------------------------------------------*/
static int gg_adv_name_cmd_func(int argc, char **argv)
{
    GG_Result rc;
    nvm_error_t err;

    if (argc != 2) {
        goto gg_adv_name_cmd_usage;
    }

    err = nvm_set_adv_name(argv[1]);

    if (err != NVM_OK) {
        console_printf("Failed to store adv name to NVM!\n");
    }

    rc = GG_ConnMgr_SetAdvertiseName(argv[1]);

    return rc == GG_SUCCESS ? 0 : 1;

gg_adv_name_cmd_usage:
    console_printf("Usage:\n  gg %s <adv_name>\n", argv[0]);
    return 1;
}

/*----------------------------------------------------------------------
|   gg command table
+---------------------------------------------------------------------*/
static ShellCmdTableEntry shell_cmd_table[] = {
    {GG_SET_LOG_CONFIG_CLI_STR, gg_set_log_config_cmd_func},
    {GG_GET_LOG_CONFIG_CLI_STR, gg_get_log_config_cmd_func},
#if MYNEWT_VAL(GG_CONNMGR_CENTRAL)
    {BT_HANDLER_CONNECT, gg_connect_cmd_func},
    {GG_AUTO_CONNECT_CMD_STRING, gg_auto_connect_cmd_func},
#endif
    {BT_HANDLER_DISCONNECT, gg_disconnect_cmd_func},
#if MYNEWT_VAL(GG_CONNMGR_PERIPHERAL)
    {BT_HANDLER_SET_ADV_STATE, gg_adv_set_state_cmd_func},
#endif
    {BT_HANDLER_SET_ADV_NAME, gg_adv_name_cmd_func},
    {BT_HANDLER_MTU_EXCHANGE, gg_mtu_update_cmd_func},
    {GG_CONN_PARAMS_CMD_STRING, gg_conn_params_cmd_func},
    {BT_HANDLER_GET_CONN_SVC_STATUS, gg_get_conn_svc_status_cmd_func},
    {BT_HANDLER_GET_CONN_CONFIG, gg_get_conn_config_cmd_func},
    {BT_HANDLER_SET_CONN_SPEED, gg_set_conn_speed_cmd_func},
    {BT_HANDLER_CONFIG_CONN_SPEEDS, gg_config_conn_speeds_cmd_func},
    {GG_STACK_SERVICE_SET_TYPE_METHOD, gg_stack_cmd_func},
    {GG_BLAST_SERVICE_START_METHOD, gg_blast_cmd_func},
    {GG_BLAST_SERVICE_STOP_METHOD, gg_blast_cmd_func},
    {GG_BLAST_SERVICE_GET_STATS_METHOD, gg_blast_cmd_func},
    {GG_BLAST_SERVICE_RESET_STATS_METHOD, gg_blast_cmd_func},
#ifdef NRF52840_XXAA
#if MYNEWT_VAL(GG_CONNMGR_CENTRAL)
    {GG_COAP_SYNC_DUMP_CMD_STRING, gg_sync_cmd_func},
    {GG_COAP_SYNC_RESPONSE_CMD_STRING, gg_sync_response_cmd_func},
#endif
    {GG_COAP_HELLOWORLD_CMD_STRING, gg_coap_helloworld_cmd_func},
    {GG_COAP_CLIENT_CMD_STRING, CoapClient_CLI_Handler},
#endif
};


static int gg_cmd_func(int argc, char **argv)
{
    if (argc < 1) {
        goto gg_cmd_usage;
    }

    for (unsigned int i = 0; i < GG_ARRAY_SIZE(shell_cmd_table); i++) {
        if (!strcmp(argv[1], shell_cmd_table[i].name)) {
            return shell_cmd_table[i].cmd(argc - 1, &argv[1]);
        }
    }

gg_cmd_usage:
    console_printf("Usage:\n");
    for (unsigned int i = 0; i < GG_ARRAY_SIZE(shell_cmd_table); i++) {
        console_printf("  %s %s\n", GG_CMD_STRING, shell_cmd_table[i].name);
    }

    return 1;
}

/*----------------------------------------------------------------------
|   gg_print_startup_info
+---------------------------------------------------------------------*/
static void gg_print_startup_info(void)
{
    char buf[64];
    uint16_t maj;
    uint16_t min;
    uint16_t patch;
    uint32_t commit_count;
    const char *commit_hash;
    const char *branch_name;
    const char *build_date;
    const char *build_time;
    nvm_error_t rc;

    GG_Version(&maj, &min, &patch, &commit_count, &commit_hash, &branch_name,
               &build_date, &build_time);

    console_printf("GG lib version: %d.%d.%d %ld %s %s %s %s\n",
                   maj, min, patch, commit_count, commit_hash, branch_name,
                   build_date, build_time);

    memset(buf, 0, sizeof(buf));
    rc = nvm_get_log_config(buf, sizeof(buf));
    if (rc == NVM_OK) {
        console_printf("Log level is set to '%s'\n", buf);
    }
}

/*----------------------------------------------------------------------
|   main
+---------------------------------------------------------------------*/
int main(void)
{
    GG_Result rc;

    sysinit();

    GG_Module_Initialize();

    gg_print_startup_info();

    rc = GG_Loop_Create(&g_loop);
    GG_ASSERT(GG_SUCCEEDED(rc));

    gg_connmgr_init();

    /*
     * Need to run loop in different task as main task is used for
     * processing event from the Mynewt default queue.
     */
    os_task_init(&g_loop_task, "gg_loop", gg_loop_task, NULL,
                 GG_LOOP_TASK_PRIORITY, OS_WAIT_FOREVER, g_loop_task_stack,
                 GG_LOOP_TASK_STACK_SIZE);

    // Now that a task has been created for the GG main loop, we can init other services
    rc = GG_BlastService_Create(g_loop, &blaster);
    if (rc != GG_SUCCESS) {
        GG_LOG_WARNING("Failed to create blast service!");
        return 1;
    }

    rc = GG_StackService_Create(&stack_service);
    if (rc != GG_SUCCESS) {
        GG_LOG_WARNING("Failed to create stack service!");
        return 1;
    }

    // Set leds
#if MYNEWT_VAL(GG_CONNMGR_PERIPHERAL)
    hal_gpio_init_out(LED_1, 0);
#else
    hal_gpio_init_out(LED_2, 0);
#endif

    reset_cmd.sc_cmd = RESET_CMD_STRING;
    reset_cmd.sc_cmd_func = reset_cmd_func;
    shell_cmd_register(&reset_cmd);

    gg_cmd.sc_cmd = GG_CMD_STRING;
    gg_cmd.sc_cmd_func = gg_cmd_func;
    shell_cmd_register(&gg_cmd);

    while (1) {
        os_eventq_run(os_eventq_dflt_get());
    }

    return 0;
}

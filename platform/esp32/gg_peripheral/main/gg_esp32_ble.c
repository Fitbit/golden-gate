//----------------------------------------------------------------------
// includes
//----------------------------------------------------------------------
#include <stdbool.h>

#include "esp_nimble_hci.h"
#include "esp_log.h"
#include "nimble/ble.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_gap.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#include "gg_esp32_ble.h"

#include "xp/common/gg_common.h"

//----------------------------------------------------------------------
// logging
//----------------------------------------------------------------------
static const char* TAG = "gg-ble-io";

//----------------------------------------------------------------------
// Class definition for receiving data from the stack
//----------------------------------------------------------------------
typedef struct {
    GG_IMPLEMENTS(GG_DataSink);

    // NOTE: no data members in this class
} BleSink;

//----------------------------------------------------------------------
// constants
//----------------------------------------------------------------------
#define GG_GATT_BUFFER_RETRY_INTERVAL   1
#define GG_GATT_BUFFER_RETRY_COUNT      1000

//----------------------------------------------------------------------
// Globals
//----------------------------------------------------------------------
static GG_Loop* gg_loop;
static BleSink  gg_ble_sink;
static uint8_t  gg_own_address_type;
static uint16_t gg_gatt_gattlink_rx_attr_handle;
static uint16_t gg_gatt_gattlink_tx_attr_handle;
static uint16_t gg_connection_handle = BLE_HS_CONN_HANDLE_NONE;
static bool     gg_link_up = false;

//----------------------------------------------------------------------
// Forward declarations
//----------------------------------------------------------------------
static bool gg_gap_start_advertisting(void);

//----------------------------------------------------------------------
// Print a Bluetooth address
//----------------------------------------------------------------------
static void
print_address(const uint8_t* address, const char* prefix)
{
    ESP_LOGI(TAG, "%s%02x:%02x:%02x:%02x:%02x:%02x",
             prefix ? prefix : "",
             address[5],
             address[4],
             address[3],
             address[2],
             address[1],
             address[0]);
}

//----------------------------------------------------------------------
// Print a Bluetooth connection descriptor
//----------------------------------------------------------------------
static void
print_conn_desc(struct ble_gap_conn_desc* desc)
{
    ESP_LOGI(TAG, "our_ota_addr_type   = %d", desc->our_ota_addr.type);
    print_address(desc->our_ota_addr.val, "our_ota_addr        = ");
    ESP_LOGI(TAG, "our_id_addr_type    = %d", desc->our_id_addr.type);
    print_address(desc->our_id_addr.val, "our_id_addr         = ");
    ESP_LOGI(TAG, "peer_ota_addr_type  = %d", desc->peer_ota_addr.type);
    print_address(desc->peer_ota_addr.val, "peer_ota_addr       = ");
    ESP_LOGI(TAG, "peer_id_addr_type   = %d", desc->peer_id_addr.type);
    print_address(desc->peer_id_addr.val, "peer_id_addr        = ");
    ESP_LOGI(TAG, "connection interval = %d", desc->conn_itvl);
    ESP_LOGI(TAG, "connection latency  = %d", desc->conn_latency);
    ESP_LOGI(TAG, "supervision_timeout = %d", desc->supervision_timeout);
}

//----------------------------------------------------------------------
// Attach the BLE sink to the bottom of a stack
//----------------------------------------------------------------------
void
gg_esp32_ble_attach_stack(GG_DataSource* source)
{
    // Connect the stack to the BLE sink
    GG_DataSource_SetDataSink(source, GG_CAST(&gg_ble_sink, GG_DataSink));
}

//----------------------------------------------------------------------
// Callback invoked when data is received on the Gattlink RX characteristic.
// This function creates a buffer to copy the data into, and invokes
// a secondary function that will process that buffer on the GG loop thread.
//----------------------------------------------------------------------
static int
gg_on_gattlink_rx(struct os_mbuf* packet)
{
    // Check that we have a loop
    if (!gg_loop) {
        return 0;
    }

    // Get the packet size
    size_t packet_size = OS_MBUF_PKTLEN(packet);

    GG_LOG_FINE("Gattlink RX: size = %u", (int)packet_size);

    // Create a buffer to copy the packet payload into
    GG_DynamicBuffer* buffer = NULL;
    GG_Result result = GG_DynamicBuffer_Create(packet_size, &buffer);
    if (GG_FAILED(result)) {
        GG_LOG_SEVERE("failed to create buffer");
        return 0;
    }

    // Copy the data
    result = GG_DynamicBuffer_SetDataSize(buffer, packet_size);
    if (GG_FAILED(result)) {
        GG_LOG_SEVERE("failed to allocate buffer space");
        GG_DynamicBuffer_Release(buffer);
        return 0;
    }
    uint8_t* buffer_data = GG_DynamicBuffer_UseData(buffer);
    int copy_result = os_mbuf_copydata(packet, 0, packet_size, buffer_data);
    if (copy_result != 0) {
        GG_LOG_SEVERE("failed to copy packet");
        return 0;
    }

    // Notify that we have received a packet
    gg_on_packet_received(GG_DynamicBuffer_AsBuffer(buffer));

    return 0;
}

//----------------------------------------------------------------------
// Callback invoked when a characteristic is accessed
//----------------------------------------------------------------------
static int
gg_on_gatt_characteristic_access(uint16_t                     conn_handle,
                                 uint16_t                     attr_handle,
                                 struct ble_gatt_access_ctxt* context,
                                 void*                        arg)
{
    if (context->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        ESP_LOGI(TAG, "GATT Write");
        if (attr_handle == gg_gatt_gattlink_rx_attr_handle) {
            return gg_on_gattlink_rx(context->om);
        }
    } else if (context->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        ESP_LOGI(TAG, "GATT Read");
        return 0;
    }

    return BLE_ATT_ERR_UNLIKELY;
}

//----------------------------------------------------------------------
// Callback invoked when a GAP event occurs
//----------------------------------------------------------------------
static int
gg_on_gap_event(struct ble_gap_event *event, void *arg)
{
    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            ESP_LOGI(TAG, "BLE_GAP_EVENT_CONNECT: connection %s, status = %d",
                     event->connect.status == 0 ? "established" : "failed",
                     event->connect.status);
            if (event->connect.status == 0) {
                // Keep the connection handle
                gg_connection_handle = event->connect.conn_handle;

                // Print the connection info
                struct ble_gap_conn_desc desc;
                int rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
                assert(rc == 0);
                print_conn_desc(&desc);
            }

            if (event->connect.status != 0) {
                // The connection failed, resume advertising
                gg_connection_handle = BLE_HS_CONN_HANDLE_NONE;
                gg_gap_start_advertisting();
            }
            break;

        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGI(TAG, "BLE_GAP_EVENT_DISCONNECT: reason = 0x%x", event->disconnect.reason);

            // Clear the connection handle
            gg_connection_handle = BLE_HS_CONN_HANDLE_NONE;

            // Connection terminated, resume advertising
            gg_gap_start_advertisting();

            // Notify that the link is down
            if (gg_link_up) {
                gg_link_up = false;
                gg_on_link_down();
            }
            break;

        case BLE_GAP_EVENT_MTU:
            ESP_LOGI(TAG, "BLE_GAP_EVENT_MTU: mtu = %d", event->mtu.value);
            gg_on_mtu_changed(event->mtu.value);
            break;

        case BLE_GAP_EVENT_CONN_UPDATE:
            ESP_LOGI(TAG, "BLE_GAP_EVENT_CONN_UPDATE");
            break;

        case BLE_GAP_EVENT_L2CAP_UPDATE_REQ:
            ESP_LOGI(TAG, "BLE_GAP_EVENT_L2CAP_UPDATE_REQ");
            break;

        case BLE_GAP_EVENT_SUBSCRIBE:
            ESP_LOGI(TAG, "BLE_GAP_EVENT_SUBSCRIBE: handle = %d, value = %d",
                     event->subscribe.attr_handle,
                     event->subscribe.cur_notify);
            if (event->subscribe.attr_handle == gg_gatt_gattlink_tx_attr_handle) {
                if (event->subscribe.cur_notify) {
                    ESP_LOGI(TAG, "Gattlink TX subscription");
                    if (!gg_link_up) {
                        gg_link_up = true;
                        gg_on_link_up();
                    }
                } else {
                    ESP_LOGI(TAG, "Gattlink TX un-subscription");
                    if (gg_link_up) {
                        gg_link_up = false;
                        gg_on_link_down();
                    }
                }
            }
            break;

        case BLE_GAP_EVENT_NOTIFY_RX:
            ESP_LOGI(TAG, "BLE_GAP_EVENT_NOTIFY_RX");
            break;

        case BLE_GAP_EVENT_NOTIFY_TX:
            ESP_LOGI(TAG, "BLE_GAP_EVENT_NOTIFY_TX");
            break;

        default:
            ESP_LOGI(TAG, "Received unhandled gap event (%d)", event->type);
            return 1;
    }

    return 0;
}

//----------------------------------------------------------------------
// Obtain a send buffer from the BLE stack
//----------------------------------------------------------------------
static struct os_mbuf*
gg_gatt_get_buffer(const uint8_t* data, size_t data_size)
{
    struct os_mbuf* buffer;

    for (int watchdog = GG_GATT_BUFFER_RETRY_COUNT; watchdog; --watchdog) {
        buffer = ble_hs_mbuf_from_flat(data, data_size);
        if (buffer) {
            return buffer;
        }

        // Wait a bit and retry
        vTaskDelay(GG_GATT_BUFFER_RETRY_INTERVAL);
    }

    GG_LOG_WARNING("no free mbuf available");
    return NULL;
}

//----------------------------------------------------------------------
// GG_DataSink::PutData implementation for the BleSink class
//----------------------------------------------------------------------
static GG_Result
BleSink_PutData(GG_DataSink* self, GG_Buffer* data, const GG_BufferMetadata* metadata)
{
    GG_COMPILER_UNUSED(self);
    GG_COMPILER_UNUSED(metadata);

    // Obtain the packet data and size
    const uint8_t* packet      = GG_Buffer_GetData(data);
    size_t         packet_size = GG_Buffer_GetDataSize(data);
    ESP_LOGI(TAG, "Got %u bytes from stack", (int)packet_size);

    // Check that we have a characteristic to notify
    if (!gg_gatt_gattlink_tx_attr_handle) {
        GG_LOG_WARNING("no TX characteristic, dropping");
        return GG_SUCCESS;
    }

    // Obtain a buffer from the BLE stack
    struct os_mbuf* buffer = gg_gatt_get_buffer(packet, packet_size);
    if (!buffer) {
        return GG_ERROR_OUT_OF_MEMORY;
    }

    // Send the data as a notification
    int result = ble_gattc_notify_custom(gg_connection_handle,
                                         gg_gatt_gattlink_tx_attr_handle,
                                         buffer);
    if (result != 0) {
        GG_LOG_WARNING("ble_gattc_notify_custom failed (0x%x)", result);
        return GG_FAILURE;
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
// GG_DataSink::SetListener implementation for the BleSink class
//----------------------------------------------------------------------
static GG_Result
BleSink_SetListener(GG_DataSink* self, GG_DataSinkListener* listener)
{
    // Nothing to do there
    GG_COMPILER_UNUSED(self);
    GG_COMPILER_UNUSED(listener);
    return GG_SUCCESS;
}

//----------------------------------------------------------------------
// BleSink vtable
//----------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(BleSink, GG_DataSink) {
    .PutData     = BleSink_PutData,
    .SetListener = BleSink_SetListener
};

//----------------------------------------------------------------------
// GATT services
//----------------------------------------------------------------------
/* ABBAFF00-E56A-484C-B832-8B17CF6CBFE8 */
static const ble_uuid128_t gg_gatt_gattlink_svc_uuid =
BLE_UUID128_INIT(
    0xE8, 0xBF, 0x6C, 0xCF, 0x17, 0x8B, 0x32, 0xB8,
    0x4C, 0x48, 0x6A, 0xE5, 0x00, 0xFF, 0xBA, 0xAB
);

/* ABBAFF01-E56A-484C-B832-8B17CF6CBFE8 */
static const ble_uuid128_t gg_gatt_gattlink_chr_rx_uuid =
BLE_UUID128_INIT(
    0xE8, 0xBF, 0x6C, 0xCF, 0x17, 0x8B, 0x32, 0xB8,
    0x4C, 0x48, 0x6A, 0xE5, 0x01, 0xFF, 0xBA, 0xAB
);

/* ABBAFF02-E56A-484C-B832-8B17CF6CBFE8 */
static const ble_uuid128_t gg_gatt_gattlink_chr_tx_uuid =
BLE_UUID128_INIT(
    0xE8, 0xBF, 0x6C, 0xCF, 0x17, 0x8B, 0x32, 0xB8,
    0x4C, 0x48, 0x6A, 0xE5, 0x02, 0xFF, 0xBA, 0xAB
);

static const struct
ble_gatt_svc_def gg_gatt_services[] = {
    {
        /*** Service: Gattlink */
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &gg_gatt_gattlink_svc_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                /*** Characteristic: Gattlink RX */
                .uuid       = &gg_gatt_gattlink_chr_rx_uuid.u,
                .val_handle = &gg_gatt_gattlink_rx_attr_handle,
                .access_cb  = gg_on_gatt_characteristic_access,
                .flags      = BLE_GATT_CHR_F_WRITE_NO_RSP
            },
            {
                /*** Characteristic: Gattlink TX */
                .uuid       = &gg_gatt_gattlink_chr_tx_uuid.u,
                .val_handle = &gg_gatt_gattlink_tx_attr_handle,
                .access_cb  = gg_on_gatt_characteristic_access,
                .flags      = BLE_GATT_CHR_F_NOTIFY
            },
            {
                0, /* No more characteristics in this service */
            }
        },
    },
    {
        0, /* No more services */
    }
};

//----------------------------------------------------------------------
// Start BLE advertising
//----------------------------------------------------------------------
static bool
gg_gap_start_advertisting()
{
    // basic fields
    struct ble_hs_adv_fields adv_fields;
    memset(&adv_fields, 0, sizeof(adv_fields));
    adv_fields.flags                 = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    adv_fields.tx_pwr_lvl_is_present = 1;
    adv_fields.tx_pwr_lvl            = BLE_HS_ADV_TX_PWR_LVL_AUTO;
    adv_fields.uuids128              = (ble_uuid128_t*)gg_gatt_services[0].uuid;
    adv_fields.num_uuids128          = 1;
    adv_fields.uuids128_is_complete  = 1;
    int rc = ble_gap_adv_set_fields(&adv_fields);
    if (rc == BLE_HS_EBUSY) {
        // advertising enabled, updates not allowed
        // this is used liberally during testing and shouldn't result in an error/failure
        ESP_LOGI(TAG, "advertising enable ignored, advertising already enabled; rc=0x%x", rc);
    } else if (rc != 0) {
        ESP_LOGE(TAG, "error setting basic advertisement data; rc=0x%x", rc);
    }

    // extended fields
    memset(&adv_fields, 0, sizeof(adv_fields));
    const char* name = ble_svc_gap_device_name();
    adv_fields.name             = (uint8_t *)name;
    adv_fields.name_len         = strlen(name);
    adv_fields.name_is_complete = 1;
    rc = ble_gap_adv_rsp_set_fields(&adv_fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "error setting extended advertisement data; rc=0x%x", rc);
        return false;
    }

    // start advertising
    struct ble_gap_adv_params adv_params;
    memset(&adv_params, 0, sizeof adv_params);
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    rc = ble_gap_adv_start(gg_own_address_type,
                           NULL,
                           BLE_HS_FOREVER,
                           &adv_params,
                           gg_on_gap_event,
                           NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "error enabling advertising; rc=0x%x", rc);
        return false;
    }

    return true;
}

//----------------------------------------------------------------------
// Setup GATT services
//----------------------------------------------------------------------
static int
gg_gatt_setup(void)
{
    ble_svc_gap_init();
    ble_svc_gatt_init();

    // Check the config
    int rc = ble_gatts_count_cfg(gg_gatt_services);
    if (rc != 0) {
        return rc;
    }

    // Add the services
    rc = ble_gatts_add_svcs(gg_gatt_services);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

//----------------------------------------------------------------------
// Callback executed when the host resets itself and the controller
// due to fatal error
//----------------------------------------------------------------------
static void
gg_on_ble_host_reset(int reason)
{
    ESP_LOGW(TAG, "Bluetooth host resetting state; reason=%d\n", reason);
}

//----------------------------------------------------------------------
// Callback executed when the host and controller become synced.
// This happens at startup and after a reset.
//----------------------------------------------------------------------
static void
gg_on_ble_host_sync(void)
{
    int rc = ble_hs_util_ensure_addr(0);
    assert(rc == 0);

    // Figure out address to use while advertising
    rc = ble_hs_id_infer_auto(0, &gg_own_address_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "error determining address type; rc=%d\n", rc);
        return;
    }

    // Print the address
    uint8_t address[6] = {0};
    rc = ble_hs_id_copy_addr(gg_own_address_type, address, NULL);
    print_address(address, "Device Address: ");

    // Start advertising
    gg_gap_start_advertisting();
}

//----------------------------------------------------------------------
// Entry point for the task in which we will run the Nimble stack
//----------------------------------------------------------------------
static void
ble_host_task(void* param)
{
    (void)param;

    // run until nimble_port_stop() is called
    ESP_LOGI(TAG, "--- starting Nimble stack task");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

//----------------------------------------------------------------------
// Initialize the ESP32 bluetooth subsystem (Nimble) and the services
// we need to run on it.
//----------------------------------------------------------------------
bool
gg_esp32_ble_init(GG_Loop* loop)
{
    // Keep references
    gg_loop = loop;

    // Init the BLE sink object
    GG_SET_INTERFACE(&gg_ble_sink, BleSink, GG_DataSink);

    // Init the Nimble host and controller
    int result = esp_nimble_hci_and_controller_init();
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "esp_nimble_hci_and_controller_init() failed with error: %d", result);
        return false;
    }

    // Init the Nimble BLE stack
    nimble_port_init();

    // Setup GATT services
    int rc = gg_gatt_setup();
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to initialize GATT services; rc=%d", rc);
    }

    // Set the advertised peripheral name
    rc = ble_svc_gap_device_name_set("gg-esp32");
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to set the peripheral name; rc=%d", rc);
    }

    // Init the NimBLE host configuration. */
    ble_hs_cfg.reset_cb = gg_on_ble_host_reset;
    ble_hs_cfg.sync_cb  = gg_on_ble_host_sync;

    // Start a task for the stack
    nimble_port_freertos_init(ble_host_task);

    return true;
}

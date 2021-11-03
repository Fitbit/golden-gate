/**
 * @file
 * @brief Mynewt Connection Manager Implementation
 *
 * @copyright
 * Copyright 2017-2020 Fitbit, Inc
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author Bogdan Davidoaia
 *
 * @date 2017-11-13
 *
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <inttypes.h>
#include <string.h>
#include <stdio.h>

#include "bsp/bsp.h"
#include "hal/hal_bsp.h"
#include "nimble/ble.h"
#include "host/ble_gap.h"
#include "host/ble_hs.h"
#include "services/gap/ble_svc_gap.h"
#include "controller/ble_hw.h"
#include "controller/ble_ll.h"

#include "xp/common/gg_common.h"

#include "nvm.h"

#include "gg_connmgr.h"

/*----------------------------------------------------------------------
|   macros and constants
+---------------------------------------------------------------------*/
#define BLE_TASK_PRIO               10
#define BLE_STACK_SIZE              (OS_STACK_ALIGN(512))

#define REMOTE_GATT_DB_MAX_SCV      4
#define REMOTE_GATT_DB_MAX_CHR      5
#define REMOTE_GATT_DB_MAX_DSC      2

#define BLE_GATTS_CLT_CFG_F_NOTIFY  0x0001

#define BLE_CONNECT_TIMEOUT         10000
#define BLE_SCAN_TIMEOUT            60000

// Values taken from fw-bluetooth/src/fb_bt_conn_params.c
// Updated fast conn interval to have min = max to get 15ms conn interval
#define GG_CONN_MODE_SLOW_MIN_INTERVAL     96  // in units of 1.25ms = 120ms
#define GG_CONN_MODE_SLOW_MAX_INTERVAL     116 // in units of 1.25ms = 145ms
#define GG_CONN_MODE_SLOW_LATENCY          3   // in units of connection intervals
#define GG_CONN_MODE_SLOW_TIMEOUT          40  // in units of 100ms = 4s

#define GG_CONN_MODE_FAST_MIN_INTERVAL     12  // in units of 1.25ms = 15ms
#define GG_CONN_MODE_FAST_MAX_INTERVAL     12  // in units of 1.25ms = 15ms
#define GG_CONN_MODE_FAST_LATENCY          0   // in units of connection intervals
#define GG_CONN_MODE_FAST_TIMEOUT          20  // in units of 100ms = 2s

#define GG_GATTLINK_L2CAP_PSM             0xC0
#define GG_GATTLINK_L2CAP_MTU             2048
#define GG_GATTLINK_L2CAP_MAX_PACKET_SIZE 256

/*----------------------------------------------------------------------
|   BLE configs
+---------------------------------------------------------------------*/
/* ABBAFF00-E56A-484C-B832-8B17CF6CBFE8 */
static const ble_uuid128_t gatt_svr_gattlink_svc_uuid =
BLE_UUID128_INIT(
    0xE8, 0xBF, 0x6C, 0xCF, 0x17, 0x8B, 0x32, 0xB8,
    0x4C, 0x48, 0x6A, 0xE5, 0x00, 0xFF, 0xBA, 0xAB
);

/* ABBAFF01-E56A-484C-B832-8B17CF6CBFE8 */
static const ble_uuid128_t gatt_svr_gattlink_chr_rx_uuid =
BLE_UUID128_INIT(
    0xE8, 0xBF, 0x6C, 0xCF, 0x17, 0x8B, 0x32, 0xB8,
    0x4C, 0x48, 0x6A, 0xE5, 0x01, 0xFF, 0xBA, 0xAB
);

/* ABBAFF02-E56A-484C-B832-8B17CF6CBFE8 */
static const ble_uuid128_t gatt_svr_gattlink_chr_tx_uuid =
BLE_UUID128_INIT(
    0xE8, 0xBF, 0x6C, 0xCF, 0x17, 0x8B, 0x32, 0xB8,
    0x4C, 0x48, 0x6A, 0xE5, 0x02, 0xFF, 0xBA, 0xAB
);

/* ABBAFF03-E56A-484C-B832-8B17CF6CBFE8 */
static const ble_uuid128_t gatt_svr_gattlink_chr_l2cap_psm_uuid =
BLE_UUID128_INIT(
    0xE8, 0xBF, 0x6C, 0xCF, 0x17, 0x8B, 0x32, 0xB8,
    0x4C, 0x48, 0x6A, 0xE5, 0x03, 0xFF, 0xBA, 0xAB
);

/* ABBAFD00-E56A-484C-B832-8B17CF6CBFE8 */
static const ble_uuid128_t gatt_svr_link_status_svc_uuid =
BLE_UUID128_INIT(
    0xE8, 0xBF, 0x6C, 0xCF, 0x17, 0x8B, 0x32, 0xB8,
    0x4C, 0x48, 0x6A, 0xE5, 0x00, 0xFD, 0xBA, 0xAB
);

/* ABBAFD01-E56A-484C-B832-8B17CF6CBFE8 */
static const ble_uuid128_t gatt_svr_link_status_connection_configuration_chr_uuid =
BLE_UUID128_INIT(
    0xE8, 0xBF, 0x6C, 0xCF, 0x17, 0x8B, 0x32, 0xB8,
    0x4C, 0x48, 0x6A, 0xE5, 0x01, 0xFD, 0xBA, 0xAB
);

/* ABBAFD02-E56A-484C-B832-8B17CF6CBFE8 */
static const ble_uuid128_t gatt_svr_link_status_connection_status_chr_uuid =
BLE_UUID128_INIT(
    0xE8, 0xBF, 0x6C, 0xCF, 0x17, 0x8B, 0x32, 0xB8,
    0x4C, 0x48, 0x6A, 0xE5, 0x02, 0xFD, 0xBA, 0xAB
);

#if MYNEWT_VAL(GG_CONNMGR_PERIPHERAL)
/* ABBAFD03-E56A-484C-B832-8B17CF6CBFE8 */
static const ble_uuid128_t gatt_svr_link_status_secure_chr_uuid =
BLE_UUID128_INIT(
    0xE8, 0xBF, 0x6C, 0xCF, 0x17, 0x8B, 0x32, 0xB8,
    0x4C, 0x48, 0x6A, 0xE5, 0x03, 0xFD, 0xBA, 0xAB
);
#endif

/* ABBAFC00-E56A-484C-B832-8B17CF6CBFE8 */
static const ble_uuid128_t gatt_svr_link_configuration_svc_uuid =
BLE_UUID128_INIT(
    0xE8, 0xBF, 0x6C, 0xCF, 0x17, 0x8B, 0x32, 0xB8,
    0x4C, 0x48, 0x6A, 0xE5, 0x00, 0xFC, 0xBA, 0xAB
);

/* ABBAFC01-E56A-484C-B832-8B17CF6CBFE8 */
static const ble_uuid128_t gatt_svr_link_configuration_connection_configuration_chr_uuid =
BLE_UUID128_INIT(
    0xE8, 0xBF, 0x6C, 0xCF, 0x17, 0x8B, 0x32, 0xB8,
    0x4C, 0x48, 0x6A, 0xE5, 0x01, 0xFC, 0xBA, 0xAB
);

/* ABBAFC02-E56A-484C-B832-8B17CF6CBFE8 */
static const ble_uuid128_t gatt_svr_link_configuration_connection_mode_chr_uuid =
BLE_UUID128_INIT(
   0xE8, 0xBF, 0x6C, 0xCF, 0x17, 0x8B, 0x32, 0xB8,
   0x4C, 0x48, 0x6A, 0xE5, 0x02, 0xFC, 0xBA, 0xAB
);

/*----------------------------------------------------------------------
|   BLE handles and state
+---------------------------------------------------------------------*/
#if MYNEWT_VAL(GG_CONNMGR_CENTRAL)
static uint16_t link_status_connection_configuration_cccd_handle;
static uint16_t link_status_connection_status_cccd_handle;
static uint16_t gattlink_tx_cccd_handle;
#else
static uint16_t link_configuration_connection_configuration_cccd_handle;
static uint16_t link_configuration_connection_mode_cccd_handle;
static uint16_t link_status_secure_chr_attr_handle;
#endif

static uint16_t link_configuration_connection_configuration_chr_attr_handle;
static uint16_t link_configuration_connection_mode_chr_attr_handle;

static uint16_t link_status_connection_configuration_chr_attr_handle;
static uint16_t link_status_connection_status_chr_attr_handle;

static uint16_t gattlink_rx_attr_handle;
static uint16_t gattlink_tx_attr_handle;
static uint16_t gattlink_l2cap_psm_attr_handle;

static struct ble_l2cap_chan* gattlink_l2cap_channel;

#if MYNEWT_VAL(GG_CONNMGR_PERIPHERAL) == 0
static uint8_t disc_ble_addr[BLE_DEV_ADDR_LEN];
static char *disc_name;
static char *g_peer_name;
static bool disc_uuid_ok;
#endif

/*----------------------------------------------------------------------
|   forward declarations
+---------------------------------------------------------------------*/
static int ble_gatt_svr_chr_access_cb(uint16_t conn_handle,
                                      uint16_t attr_handle,
                                      struct ble_gatt_access_ctxt *ctxt,
                                      void* arg);
static void ble_pump_gatt_operation_queue(void);

/*----------------------------------------------------------------------
|   services tables
+---------------------------------------------------------------------*/
#if MYNEWT_VAL(GG_CONNMGR_PERIPHERAL)

static const uint8_t ble_dev_addr[6] = {0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B};
static const char *gap_device_name = "gg-peripheral";
static char* speed_str[] = {"fast", "slow"};

/* GG Peripheral Services */
static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    {
        /*** Service: Gattlink */
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &gatt_svr_gattlink_svc_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                /*** Characteristic: Gattlink RX */
                .uuid       = &gatt_svr_gattlink_chr_rx_uuid.u,
                .val_handle = &gattlink_rx_attr_handle,
                .access_cb  = ble_gatt_svr_chr_access_cb,
                .flags      = BLE_GATT_CHR_F_WRITE_NO_RSP
            },
            {
                /*** Characteristic: Gattlink TX */
                .uuid       = &gatt_svr_gattlink_chr_tx_uuid.u,
                .val_handle = &gattlink_tx_attr_handle,
                .access_cb  = ble_gatt_svr_chr_access_cb,
                .flags      = BLE_GATT_CHR_F_NOTIFY
            },
            {
                /*** Characteristic: L2CAP CoC PSM */
                .uuid       = &gatt_svr_gattlink_chr_l2cap_psm_uuid.u,
                .val_handle = &gattlink_l2cap_psm_attr_handle,
                .access_cb  = ble_gatt_svr_chr_access_cb,
                .flags      = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY
            },
            {
                0, /* No more characteristics in this service */
            }
        },
    },
    {
        /*** Service: GG Link Status Service */
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &gatt_svr_link_status_svc_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                /*** Characteristic: Connection Configuration */
                .uuid       = &gatt_svr_link_status_connection_configuration_chr_uuid.u,
                .val_handle = &link_status_connection_configuration_chr_attr_handle,
                .access_cb  = ble_gatt_svr_chr_access_cb,
                .flags      = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY
            },
            {
                /*** Characteristic: Connection Status */
                .uuid       = &gatt_svr_link_status_connection_status_chr_uuid.u,
                .val_handle = &link_status_connection_status_chr_attr_handle,
                .access_cb  = ble_gatt_svr_chr_access_cb,
                .flags      = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY
            },
            {
                /*** Characteristic: Secure */
                .uuid       = &gatt_svr_link_status_secure_chr_uuid.u,
                .val_handle = &link_status_secure_chr_attr_handle,
                .access_cb  = ble_gatt_svr_chr_access_cb,
                .flags      = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_READ_AUTHEN
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

#elif MYNEWT_VAL(GG_CONNMGR_CENTRAL)

static const uint8_t ble_dev_addr[6] = {0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C};
static const char *gap_device_name = "gg-central";

/* GG Central Services */
static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    {
        /*** Service: Link Configuration */
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &gatt_svr_link_configuration_svc_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                /*** Characteristic: Connection Configuration */
                .uuid       = &gatt_svr_link_configuration_connection_configuration_chr_uuid.u,
                .val_handle = &link_configuration_connection_configuration_chr_attr_handle,
                .access_cb  = ble_gatt_svr_chr_access_cb,
                .flags      = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY
            },
            {
                /*** Characteristic: Connection Mode */
                .uuid       = &gatt_svr_link_configuration_connection_mode_chr_uuid.u,
                .val_handle = &link_configuration_connection_mode_chr_attr_handle,
                .access_cb  = ble_gatt_svr_chr_access_cb,
                .flags      = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY
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
#endif

static char* connection_mode_str[] = {"default", "fast", "slow"};

/*----------------------------------------------------------------------
|   logging
+---------------------------------------------------------------------*/
GG_SET_LOCAL_LOGGER("xp.mynewt.connmgr")

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
typedef struct {
    GG_IMPLEMENTS(GG_DataSink);
    GG_IMPLEMENTS(GG_DataSource);

    GG_Loop*             loop;
    GG_DataSinkListener* listener;
    GG_DataSink*         sink;
} GG_ConnMgr;

/*----------------------------------------------------------------------
|   globals
+---------------------------------------------------------------------*/
static GG_ConnMgr                           g_connmgr;          // connection manager object
static GG_ConnMgr_Client_Callback_Functions g_client_cbs;       // client callbacks
static GG_ConnMgrState                      g_conn_state;       // Connection manager state
static GG_LinkStatus_ConnectionConfig       g_conn_config;      // Link Status Service current connection config
static GG_LinkStatus_ConnectionStatus       g_conn_status;      // Link Status Service current connection status

static uint16_t ble_conn_handle = BLE_HS_CONN_HANDLE_NONE;
static bool g_gattc_discovery_failed;

static enum {
    GATTLINK_MODE_UNKNOWN,
    GATTLINK_MODE_GATT,
    GATTLINK_MODE_L2CAP
} g_gattlink_mode = GATTLINK_MODE_UNKNOWN;

static uint8_t      g_gattlink_l2cap_packet[GG_GATTLINK_L2CAP_MAX_PACKET_SIZE];
static unsigned int g_gattlink_l2cap_packet_size;
static unsigned int g_gattlink_l2cap_packet_bytes_needed;

static GG_LinkConfiguration_ConnectionConfig g_preferred_conn_config = {
    .mask = GG_LINK_CONFIGURATION_CONNECTION_CONFIG_HAS_FAST_MODE_CONFIG |
            GG_LINK_CONFIGURATION_CONNECTION_CONFIG_HAS_SLOW_MODE_CONFIG,
    .fast_mode_config = {
        .min_connection_interval = GG_CONN_MODE_FAST_MIN_INTERVAL,
        .max_connection_interval = GG_CONN_MODE_FAST_MAX_INTERVAL,
        .slave_latency           = GG_CONN_MODE_FAST_LATENCY,
        .supervision_timeout     = GG_CONN_MODE_FAST_TIMEOUT
    },
    .slow_mode_config = {
        .min_connection_interval = GG_CONN_MODE_SLOW_MIN_INTERVAL,
        .max_connection_interval = GG_CONN_MODE_SLOW_MAX_INTERVAL,
        .slave_latency           = GG_CONN_MODE_SLOW_LATENCY,
        .supervision_timeout     = GG_CONN_MODE_SLOW_TIMEOUT
    }
};

#if MYNEWT_VAL(GG_CONNMGR_CENTRAL)
static GG_LinkConfiguration_ConnectionMode g_preferred_conn_mode = {
    .speed = (uint8_t)GG_LinkConfiguration_ConnectionSpeed_Fast
};
#endif

static bool g_connmgr_advertise_on_disconnect = true;

/*----------------------------------------------------------------------
|   remote GATT database
+---------------------------------------------------------------------*/
struct remote_gatt_db;
struct remote_gatt_svc;
struct remote_gatt_chr;
struct remote_gatt_dsc;

struct remote_gatt_dsc {
    struct ble_gatt_dsc dsc;
    struct remote_gatt_chr *chr;
    int idx;
};

struct remote_gatt_chr {
    struct ble_gatt_chr chr;
    struct remote_gatt_svc *svc;
    int idx;

    struct remote_gatt_dsc dsc[REMOTE_GATT_DB_MAX_DSC];
    int num_dsc;
};

struct remote_gatt_svc {
    struct ble_gatt_svc svc;
    int idx;

    struct remote_gatt_chr chr[REMOTE_GATT_DB_MAX_CHR];
    int num_chr;
};

struct remote_gatt_db {
    struct remote_gatt_svc svc[REMOTE_GATT_DB_MAX_SCV];
    int num_svc;
};

static struct remote_gatt_db remote_gatt_db;

/*----------------------------------------------------------------------
|   GATT request queue
+---------------------------------------------------------------------*/
#define BLE_GATT_OPERATION_QUEUE_SIZE 8

typedef int (*ble_gatt_operation_callback)(struct ble_gatt_attr* attribute);

typedef struct {
    enum {
        BLE_GATT_OPERATION_SUBSCRIBE,
        BLE_GATT_OPERATION_READ
    } type;
    uint16_t handle;
    ble_gatt_operation_callback callback;
} ble_gatt_operation;

static struct {
    struct os_mutex    mutex;
    bool               busy;
    ble_gatt_operation operations[BLE_GATT_OPERATION_QUEUE_SIZE];
    unsigned int       head;
    unsigned int       entry_count;
} g_ble_gatt_operation_queue;

/*----------------------------------------------------------------------
|   forward declarations
+---------------------------------------------------------------------*/
static struct os_mbuf* ble_get_gatt_mbuf(const uint8_t *data_buf, size_t data_len);
static struct os_mbuf* ble_get_l2cap_mbuf(const uint8_t *data_buf, size_t data_len);
static void ble_disc_chr_dscs(struct remote_gatt_chr *chr);
static void ble_disc_svc_chrs(struct remote_gatt_svc *svc);
static void ble_on_data_recv(struct os_mbuf *om);
static int ble_gap_handle_event(struct ble_gap_event *event, void *arg);
static int ble_subscribe_cb(struct ble_gatt_attr* attribute);
static int ble_read_cb(struct ble_gatt_attr* attribute);

/*----------------------------------------------------------------------
|   Data Sink interface
+---------------------------------------------------------------------*/
static GG_Result GG_ConnMgr_DataSink_PutData(GG_DataSink*             self,
                                             GG_Buffer*               data,
                                             const GG_BufferMetadata* metadata)
{
    const uint8_t *data_buf = GG_Buffer_GetData(data);
    size_t data_len = GG_Buffer_GetDataSize(data);
    struct os_mbuf* om;
    int rc = 0;

    GG_LOG_FINE("Sending data, size=%u", data_len);

#if MYNEWT_VAL(GG_CONNMGR_CENTRAL)
    if (g_gattlink_mode == GATTLINK_MODE_GATT) {
        if (gattlink_rx_attr_handle) {
            om = ble_get_gatt_mbuf(data_buf, data_len);
            if (om == NULL) {
                return GG_FAILURE;
            }
            rc = ble_gattc_write_no_rsp(ble_conn_handle, gattlink_rx_attr_handle, om);
        } else {
            GG_LOG_WARNING("no RX characteristic, dropping");
            return GG_SUCCESS;
        }
    } else if (g_gattlink_mode == GATTLINK_MODE_L2CAP) {
        // not implemented yet
    }
#else
    if (g_gattlink_mode == GATTLINK_MODE_GATT) {
        if (gattlink_tx_attr_handle) {
            om = ble_get_gatt_mbuf(data_buf, data_len);
            if (om == NULL) {
                return GG_FAILURE;
            }
            rc = ble_gattc_notify_custom(ble_conn_handle, gattlink_tx_attr_handle, om);
        } else {
            GG_LOG_WARNING("no TX characteristic, dropping");
            return GG_SUCCESS;
        }
    } else if (g_gattlink_mode == GATTLINK_MODE_L2CAP) {
        if (gattlink_l2cap_channel) {
            om = ble_get_l2cap_mbuf(data_buf, data_len);
            if (om == NULL) {
                return GG_FAILURE;
            }
            rc = ble_l2cap_send(gattlink_l2cap_channel, om);
        } else {
            GG_LOG_WARNING("no L2CAP channel, dropping");
            return GG_SUCCESS;
        }
    }
#endif

    if (rc != 0) {
        GG_LOG_WARNING("ble send data function failed (rc=0x%x)", rc);
        return GG_FAILURE;
    }

    return GG_SUCCESS;
}

static GG_Result GG_ConnMgr_DataSink_SetListener(GG_DataSink* self,
                                                 GG_DataSinkListener* listener)
{
    g_connmgr.listener = listener;

    return GG_SUCCESS;
}

GG_IMPLEMENT_INTERFACE(GG_ConnMgr, GG_DataSink) {
    GG_ConnMgr_DataSink_PutData,
    GG_ConnMgr_DataSink_SetListener,
};

/*----------------------------------------------------------------------
|   DataSource interface
+---------------------------------------------------------------------*/
static GG_Result GG_ConnMgr_DataSource_SetDataSink(GG_DataSource* self,
                                                   GG_DataSink* sink)
{
    g_connmgr.sink = sink;

    return GG_SUCCESS;
}

GG_IMPLEMENT_INTERFACE(GG_ConnMgr, GG_DataSource) {
    GG_ConnMgr_DataSource_SetDataSink,
};

/*----------------------------------------------------------------------
|   utils
+---------------------------------------------------------------------*/

//----------------------------------------------------------------------
static void log_remote_gatt_db(void)
{
    char buf[BLE_UUID_STR_LEN];
    struct remote_gatt_svc *svc;
    struct remote_gatt_chr *chr;
    struct remote_gatt_dsc *dsc;

    GG_LOG_FINE("Remote GATT database (%u services):", remote_gatt_db.num_svc);

    for (int i = 0; i < remote_gatt_db.num_svc; i++) {
        svc = &remote_gatt_db.svc[i];

        GG_LOG_FINE("service[%d] {", i);
        GG_LOG_FINE("    uuid=%s", ble_uuid_to_str(&svc->svc.uuid.u, buf));
        GG_LOG_FINE("    start_handle=0x%04x", svc->svc.start_handle);
        GG_LOG_FINE("    end_handle=0x%04x", svc->svc.end_handle);

        for (int j = 0; j < svc->num_chr; j++) {
            chr = &svc->chr[j];

            GG_LOG_FINE("    characteristic[%d] {", j);
            GG_LOG_FINE("        uuid=%s", ble_uuid_to_str(&chr->chr.uuid.u, buf));
            GG_LOG_FINE("        def_handle=0x%04x", chr->chr.def_handle);
            GG_LOG_FINE("        val_handle=0x%04x", chr->chr.val_handle);
            GG_LOG_FINE("        properties=0x%02x", chr->chr.properties);

            for (int k = 0; k < chr->num_dsc; k++) {
                dsc = &chr->dsc[k];

                GG_LOG_FINE("        descriptor[%d] {", k);
                GG_LOG_FINE("            uuid=%s", ble_uuid_to_str(&dsc->dsc.uuid.u, buf));
                GG_LOG_FINE("            handle=0x%04x", dsc->dsc.handle);
                GG_LOG_FINE("        }");
            }

            GG_LOG_FINE("    }");
        }

        GG_LOG_FINE("}");
    }
}

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

//----------------------------------------------------------------------
static struct os_mbuf* ble_get_gatt_mbuf(const uint8_t* data_buf, size_t data_len)
{
    unsigned int sleep_ticks = 1;
    int watchdog = 1000;
    struct os_mbuf *om;

    do {
        om = ble_hs_mbuf_from_flat(data_buf, data_len);
        if (!om) {
            if (--watchdog == 0) {
               GG_LOG_WARNING("no free mbuf available");
               return NULL;
            }

            // wait a bit and retry
            os_time_delay(sleep_ticks);
        }
    } while (!om);

    return om;
}

//----------------------------------------------------------------------
static struct os_mbuf* ble_get_l2cap_mbuf(const uint8_t* data_buf, size_t data_len)
{
    unsigned int sleep_ticks = 1;
    int watchdog = 1000;
    struct os_mbuf *om;

    do {
        om = os_msys_get_pkthdr(data_len + 1, 0);;
        if (!om) {
            if (--watchdog == 0) {
               GG_LOG_WARNING("no free mbuf available");
               return NULL;
            }

            // wait a bit and retry
            os_time_delay(sleep_ticks);
        }
    } while (!om);

    // copy the data with a 1-byte length_minus_one header
    uint8_t header = (uint8_t)(data_len - 1);
    os_mbuf_copyinto(om, 0, &header, 1);
    os_mbuf_copyinto(om, 1, data_buf, data_len);

    return om;
}

//----------------------------------------------------------------------
static uint16_t ble_get_remote_chr_val_handle(const ble_uuid_t *svc_uuid,
                                              const ble_uuid_t *chr_uuid)
{
    struct remote_gatt_svc *svc;
    struct remote_gatt_chr *chr;

    for (int i = 0; i < remote_gatt_db.num_svc; i++) {
        svc = &remote_gatt_db.svc[i];

        if (ble_uuid_cmp(&svc->svc.uuid.u, svc_uuid)) {
            continue;
        }

        for (int j = 0; j < svc->num_chr; j++) {
            chr = &svc->chr[j];

            if (ble_uuid_cmp(&chr->chr.uuid.u, chr_uuid)) {
                continue;
            }

            return chr->chr.val_handle;
        }
    }

    return 0;
}

//----------------------------------------------------------------------
// Called back when queued GATT operation has completed
//----------------------------------------------------------------------
static int ble_gatt_operation_queue_cb(uint16_t                     conn_handle,
                                       const struct ble_gatt_error* error,
                                       struct ble_gatt_attr*        attr,
                                       void*                        arg)
{
    GG_LOG_FINER("GATT operation callback for handle 0x%x", attr->handle);

    os_mutex_pend(&g_ble_gatt_operation_queue.mutex, OS_TIMEOUT_NEVER);

    // done with this operation
    g_ble_gatt_operation_queue.busy = false;

    // check if there are more subscriptions pending
    ble_pump_gatt_operation_queue();

    os_mutex_release(&g_ble_gatt_operation_queue.mutex);

    if (error != NULL && error->status != 0) {
        GG_LOG_WARNING("GATT operation error (status=0x%0x handle=0x%x)", error->status, error->att_handle);
        return 1;
    }

    // call back to notify that the operation has completed
    ble_gatt_operation_callback callback = arg;
    return callback(attr);
}

//----------------------------------------------------------------------
// Perform the next GATT operation in the queue if there's one
// NOTE: this function must be called with the queue mutex locked
//----------------------------------------------------------------------
static void ble_pump_gatt_operation_queue(void)
{
    while (!g_ble_gatt_operation_queue.busy && g_ble_gatt_operation_queue.entry_count) {
        // get the next operation from the queue
        ble_gatt_operation* operation = &g_ble_gatt_operation_queue.operations[g_ble_gatt_operation_queue.head];
        g_ble_gatt_operation_queue.head = (g_ble_gatt_operation_queue.head + 1) % BLE_GATT_OPERATION_QUEUE_SIZE;
        --g_ble_gatt_operation_queue.entry_count;

        switch (operation->type) {
            case BLE_GATT_OPERATION_SUBSCRIBE: {
                uint16_t cccd_notify = BLE_GATTS_CLT_CFG_F_NOTIFY;
                GG_LOG_FINER("writing to CCCD for handle 0x%x", operation->handle);
                int rc = ble_gattc_write_flat(ble_conn_handle, operation->handle,
                                              &cccd_notify, sizeof(cccd_notify),
                                              ble_gatt_operation_queue_cb,
                                              operation->callback);
                if (rc) {
                    GG_LOG_WARNING("ble_gattc_write_flat failed for handle 0x%x (%d)", operation->handle, rc);
                    continue;
                }
                break;
            }

            case BLE_GATT_OPERATION_READ:
                GG_LOG_FINER("reading characteristic with handle 0x%x", operation->handle);
                int rc = ble_gattc_read(ble_conn_handle,
                                        operation->handle,
                                        ble_gatt_operation_queue_cb,
                                        operation->callback);
                if (rc) {
                    GG_LOG_WARNING("ble_gattc_read failed for handle 0x%x (%d)", operation->handle, rc);
                    continue;
                }
                break;

            default:
                break;
        }

        g_ble_gatt_operation_queue.busy = true;
    }
}

//----------------------------------------------------------------------
// Queue a GATT operation
//----------------------------------------------------------------------
static int ble_queue_gatt_operation(const ble_gatt_operation* operation)
{
    GG_LOG_FINER("queuing GATT operation, type=%d handle 0x%x", operation->type, operation->handle);

    os_mutex_pend(&g_ble_gatt_operation_queue.mutex, OS_TIMEOUT_NEVER);
    if (g_ble_gatt_operation_queue.entry_count == BLE_GATT_OPERATION_QUEUE_SIZE) {
        // full
        GG_LOG_SEVERE("GATT operation queue full");
        os_mutex_release(&g_ble_gatt_operation_queue.mutex);
        return 1;
    }

    // add the operation to the queue
    unsigned int write_position =
        (g_ble_gatt_operation_queue.head + g_ble_gatt_operation_queue.entry_count) % BLE_GATT_OPERATION_QUEUE_SIZE;
    g_ble_gatt_operation_queue.operations[write_position] = *operation;
    ++g_ble_gatt_operation_queue.entry_count;

    // ensure that the pump is running
    ble_pump_gatt_operation_queue();

    os_mutex_release(&g_ble_gatt_operation_queue.mutex);

    return 0;
}

//----------------------------------------------------------------------
static int ble_subscribe_to_remote_chr(const ble_uuid_t *svc_uuid,
                                       const ble_uuid_t *chr_uuid,
                                       uint16_t *cccd_handle)
{
    struct remote_gatt_svc *svc;
    struct remote_gatt_chr *chr;
    struct remote_gatt_dsc *dsc;

    for (int i = 0; i < remote_gatt_db.num_svc; i++) {
        svc = &remote_gatt_db.svc[i];

        if (ble_uuid_cmp(&svc->svc.uuid.u, svc_uuid)) {
            continue;
        }

        for (int j = 0; j < svc->num_chr; j++) {
            chr = &svc->chr[j];

            if (ble_uuid_cmp(&chr->chr.uuid.u, chr_uuid)) {
                continue;
            }

            for (int k = 0; k < chr->num_dsc; k++) {
                dsc = &chr->dsc[k];

                if (!(dsc->dsc.uuid.u.type == BLE_UUID_TYPE_16) ||
                    !(dsc->dsc.uuid.u16.value == BLE_GATT_DSC_CLT_CFG_UUID16)) {
                    continue;
                }

                *cccd_handle = dsc->dsc.handle;
                ble_gatt_operation operation = {
                    .type     = BLE_GATT_OPERATION_SUBSCRIBE,
                    .handle   = dsc->dsc.handle,
                    .callback = ble_subscribe_cb
                };
                ble_queue_gatt_operation(&operation);
                return 0;
            }
        }
    }

    return BLE_HS_EINVAL;
}

//----------------------------------------------------------------------
static void ble_on_discovery_done(uint16_t status)
{
    int rc;

    if (status != 0) {
        GG_LOG_WARNING("Remote Service Discovery failed (status=0x%x)", status);
        g_gattc_discovery_failed = true;
        return;
    }

    GG_LOG_INFO("Remote Service Discovery successful");

    log_remote_gatt_db();

#if MYNEWT_VAL(GG_CONNMGR_PERIPHERAL)
    // get and subscribe to the Link Configuration connection configuration characteristic
    link_configuration_connection_configuration_chr_attr_handle =
        ble_get_remote_chr_val_handle(&gatt_svr_link_configuration_svc_uuid.u,
                                      &gatt_svr_link_configuration_connection_configuration_chr_uuid.u);
    if (link_configuration_connection_configuration_chr_attr_handle) {
        GG_LOG_INFO("Subscribing to Link Configuration connection configuration");
        rc = ble_subscribe_to_remote_chr(&gatt_svr_link_configuration_svc_uuid.u,
                                         &gatt_svr_link_configuration_connection_configuration_chr_uuid.u,
                                         &link_configuration_connection_configuration_cccd_handle);
        if (rc) {
            GG_LOG_WARNING("Failed to subscribe to Link Configuration connection configuration (rc=0x%x)", rc);
        }
    }

    // get and subscribe to the Link Configuration connection mode characteristic
    link_configuration_connection_mode_chr_attr_handle =
        ble_get_remote_chr_val_handle(&gatt_svr_link_configuration_svc_uuid.u,
                                      &gatt_svr_link_configuration_connection_mode_chr_uuid.u);
    if (link_configuration_connection_mode_chr_attr_handle) {
        GG_LOG_INFO("Subscribing to Link Configuration connection mode");
        rc = ble_subscribe_to_remote_chr(&gatt_svr_link_configuration_svc_uuid.u,
                                         &gatt_svr_link_configuration_connection_mode_chr_uuid.u,
                                         &link_configuration_connection_mode_cccd_handle);
        if (rc) {
            GG_LOG_WARNING("Failed to subscribe to Link Configuration connection mode (rc=0x%x)", rc);
        }
    }
#elif MYNEWT_VAL(GG_CONNMGR_CENTRAL)
    // get the Gattlink TX characteristic
    gattlink_rx_attr_handle = ble_get_remote_chr_val_handle(&gatt_svr_gattlink_svc_uuid.u,
                                                            &gatt_svr_gattlink_chr_rx_uuid.u);
    if (gattlink_rx_attr_handle) {
        GG_LOG_INFO("Gattlink RX found");

        // get and subscribe to the Gattlink TX characteristic
        gattlink_tx_attr_handle = ble_get_remote_chr_val_handle(&gatt_svr_gattlink_svc_uuid.u,
                                                                &gatt_svr_gattlink_chr_tx_uuid.u);
        if (gattlink_tx_attr_handle) {
            GG_LOG_INFO("Gattlink TX found, subscribing");
            rc = ble_subscribe_to_remote_chr(&gatt_svr_gattlink_svc_uuid.u,
                                             &gatt_svr_gattlink_chr_tx_uuid.u,
                                             &gattlink_tx_cccd_handle);
            if (rc) {
                GG_LOG_WARNING("Failed to subscribe to Gattlink TX (rc=0x%x)", rc);
                gattlink_tx_attr_handle = 0;
            }
        } else {
            GG_LOG_WARNING("Failed to find Gattlink TX");
        }
    } else {
        GG_LOG_WARNING("Failed to find Gattlink RX");
    }

    // get and subscribe to the Link Status connection configuration characteristic
    link_status_connection_configuration_chr_attr_handle =
        ble_get_remote_chr_val_handle(&gatt_svr_link_status_svc_uuid.u,
                                      &gatt_svr_link_status_connection_configuration_chr_uuid.u);
    if (link_status_connection_configuration_chr_attr_handle) {
        GG_LOG_INFO("Link Status connection configuration found, subscribing");
        rc = ble_subscribe_to_remote_chr(&gatt_svr_link_status_svc_uuid.u,
                                        &gatt_svr_link_status_connection_configuration_chr_uuid.u,
                                        &link_status_connection_configuration_cccd_handle);
        if (rc) {
            GG_LOG_WARNING("Failed to subscribe to Link Status connection configuration (rc=0x%x)", rc);
        }
    }

    // get and subscribe to the Link Status connection status characteristic
    link_status_connection_status_chr_attr_handle =
        ble_get_remote_chr_val_handle(&gatt_svr_link_status_svc_uuid.u,
                                      &gatt_svr_link_status_connection_status_chr_uuid.u);
    if (link_status_connection_status_chr_attr_handle) {
        GG_LOG_INFO("Link Status connection status found, subscribing");
        rc = ble_subscribe_to_remote_chr(&gatt_svr_link_status_svc_uuid.u,
                                        &gatt_svr_link_status_connection_status_chr_uuid.u,
                                        &link_status_connection_status_cccd_handle);
        if (rc) {
            GG_LOG_WARNING("Failed to subscribe to Link Status connection status (rc=0x%x)", rc);
        }
    }
#endif
}

/*----------------------------------------------------------------------
|   remote GATT discovery
+---------------------------------------------------------------------*/

//----------------------------------------------------------------------
static bool keep_dsc_in_remote_db(const ble_uuid_t *chr_uuid,
                                  const ble_uuid_t *dsc_uuid)
{
    const ble_uuid16_t cccd_uuid = BLE_UUID16_INIT(0x2902);

    if (!ble_uuid_cmp(chr_uuid, dsc_uuid)) {
        // Keep characteristic value descriptor
        return true;
    }

    // Keep CCCD
    return !ble_uuid_cmp(dsc_uuid, &cccd_uuid.u);
}

//----------------------------------------------------------------------
static int ble_on_chr_dsc_discovered(uint16_t conn_handle,
                                     const struct ble_gatt_error* error,
                                     uint16_t chr_def_handle,
                                     const struct ble_gatt_dsc* dsc,
                                     void* arg)
{
    struct remote_gatt_chr *chr = (struct remote_gatt_chr *)arg;
    struct remote_gatt_svc *svc;
    char buf[BLE_UUID_STR_LEN];
    int new_idx;

    if (g_gattc_discovery_failed) {
        return 0;
    }

    switch (error->status) {
        case 0:
            if (!keep_dsc_in_remote_db(&chr->chr.uuid.u, &dsc->uuid.u)) {
                break;
            }

            new_idx = chr->num_dsc;

            if (new_idx == REMOTE_GATT_DB_MAX_DSC) {
                GG_LOG_WARNING("Discovered more descriptors than "
                               "REMOTE_GATT_DB_MAX_DSC; dropping dsc %s",
                               ble_uuid_to_str(&dsc->uuid.u, buf));
                ble_on_discovery_done(BLE_HS_ENOMEM);
                break;
            }

            chr->dsc[new_idx].dsc = *dsc;
            chr->dsc[new_idx].idx = new_idx;
            chr->dsc[new_idx].chr = chr;
            chr->num_dsc++;

            break;

        case BLE_HS_EDONE:
            svc = chr->svc;
            if (chr->idx + 1 == svc->num_chr) {
                /* last characteristic in service */

                if (svc->idx + 1 == remote_gatt_db.num_svc) {
                    /* last service in database */
                    ble_on_discovery_done(0);
                    break;
                } else {
                    /* fist characteristic of next service */
                    svc = &remote_gatt_db.svc[svc->idx + 1];
                    chr = &svc->chr[0];
                }

            } else {
                /* next characteristic in same service */
                chr = &chr->svc->chr[chr->idx + 1];
            }

            ble_disc_chr_dscs(chr);
            break;

        default:
            ble_on_discovery_done(error->status);
    }

    return 0;
}

//----------------------------------------------------------------------
static bool keep_chr_in_remote_db(const ble_uuid_t *svc_uuid,
                                  const ble_uuid_t *chr_uuid)
{
    const ble_uuid16_t gap_uuid = BLE_UUID16_INIT(0x1800);
    const ble_uuid16_t name_uuid = BLE_UUID16_INIT(0x2A00);
    const ble_uuid16_t appearance_uuid = BLE_UUID16_INIT(0x2A01);
    const ble_uuid16_t pref_conn_params_uuid = BLE_UUID16_INIT(0x2A04);

    if (ble_uuid_cmp(svc_uuid, &gap_uuid.u)) {
        // Keep all characteristics of non-GAP services */
        return true;
    }

    // Keep only some of the GAP characteristics
    return !ble_uuid_cmp(chr_uuid, &name_uuid.u) ||
           !ble_uuid_cmp(chr_uuid, &appearance_uuid.u) ||
           !ble_uuid_cmp(chr_uuid, &pref_conn_params_uuid.u);
}

//----------------------------------------------------------------------
static int ble_on_chr_discovered(uint16_t conn_handle,
                                 const struct ble_gatt_error* error,
                                 const struct ble_gatt_chr* chr,
                                 void* arg)
{
    struct remote_gatt_svc *svc = (struct remote_gatt_svc *)arg;
    char buf[BLE_UUID_STR_LEN];
    int new_idx;

    if (g_gattc_discovery_failed) {
        return 0;
    }

    switch (error->status) {
        case 0:
            if (!keep_chr_in_remote_db(&svc->svc.uuid.u, &chr->uuid.u)) {
                break;
            }

            new_idx = svc->num_chr;

            if (new_idx == REMOTE_GATT_DB_MAX_CHR) {
                GG_LOG_SEVERE("Discovered more characteristics than "
                               "REMOTE_GATT_DB_MAX_CHR; dropping chr %s",
                               ble_uuid_to_str(&chr->uuid.u, buf));
                break;
            }

            svc->chr[new_idx].chr = *chr;
            svc->chr[new_idx].idx = new_idx;
            svc->chr[new_idx].svc = svc;
            svc->chr[new_idx].num_dsc = 0;
            svc->num_chr++;
            break;

        case BLE_HS_EDONE:
            if (svc->idx + 1 == remote_gatt_db.num_svc) {
                ble_disc_chr_dscs(&remote_gatt_db.svc[0].chr[0]);
            } else {
                ble_disc_svc_chrs(&remote_gatt_db.svc[svc->idx + 1]);
            }
            break;

        default:
            ble_on_discovery_done(error->status);
    }

    return 0;
}


//----------------------------------------------------------------------
static bool keep_svc_in_remote_db(const ble_uuid_t *svc_uuid)
{
    const ble_uuid16_t gap_uuid = BLE_UUID16_INIT(0x1800);

    // Keep GAP service
    if (!ble_uuid_cmp(svc_uuid, &gap_uuid.u)) {
        return false;
    }

#if MYNEWT_VAL(GG_CONNMGR_PERIPHERAL)
    // keep the Link Configuration service
    return (!ble_uuid_cmp(svc_uuid, &gatt_svr_link_configuration_svc_uuid.u));
#elif MYNEWT_VAL(GG_CONNMGR_CENTRAL)
    // keep the Gattlink service and the Link Status service
    return (!ble_uuid_cmp(svc_uuid, &gatt_svr_gattlink_svc_uuid.u)) ||
           (!ble_uuid_cmp(svc_uuid, &gatt_svr_link_status_svc_uuid.u));
#else
    return false;
#endif
}

//----------------------------------------------------------------------
static int ble_on_service_discovered(uint16_t conn_handle,
                                     const struct ble_gatt_error* error,
                                     const struct ble_gatt_svc* service,
                                     void* arg)
{
    char buf[BLE_UUID_STR_LEN];
    int new_idx;

    if (g_gattc_discovery_failed) {
        return 0;
    }

    switch (error->status) {
        case 0:
            if (!keep_svc_in_remote_db(&service->uuid.u)) {
                GG_LOG_FINE("Skipping discovered service %s", ble_uuid_to_str(&service->uuid.u, buf));
                break;
            }
            GG_LOG_FINE("Discovered service %u: %s", remote_gatt_db.num_svc, ble_uuid_to_str(&service->uuid.u, buf));

            /* this callback can be called multiple times with the same uuid */
            new_idx = remote_gatt_db.num_svc;

            if (new_idx != 0 && !ble_uuid_cmp(&remote_gatt_db.svc[new_idx - 1].svc.uuid.u, &service->uuid.u)) {
                break;
            }

            if (new_idx == REMOTE_GATT_DB_MAX_SCV) {
                GG_LOG_WARNING("Discovered more services than REMOTE_GATT_DB_MAX_SCV; dropping svc %s",
                               ble_uuid_to_str(&service->uuid.u, buf));
                ble_on_discovery_done(BLE_HS_ENOMEM);
                break;
            }

            remote_gatt_db.svc[new_idx].svc = *service;
            remote_gatt_db.svc[new_idx].idx = new_idx;
            remote_gatt_db.svc[new_idx].num_chr = 0;
            remote_gatt_db.num_svc++;

            break;

        case BLE_HS_EDONE:
            if (remote_gatt_db.num_svc) {
                ble_disc_svc_chrs(&remote_gatt_db.svc[0]);
            }
            break;

        default:
            ble_on_discovery_done(error->status);
    }

    return 0;
}

//----------------------------------------------------------------------
static void ble_disc_chr_dscs(struct remote_gatt_chr *chr)
{
    uint16_t start_handle;
    uint16_t end_handle;
    int result;

    start_handle = chr->chr.def_handle;

    if (chr->idx + 1 == chr->svc->num_chr) {
        /* last characteristic in service, so use service end handle */
        end_handle = chr->svc->svc.end_handle;
    } else {
        /* use def_handle - 1 of next characteristic in service */
        end_handle = chr->svc->chr[chr->idx + 1].chr.def_handle - 1;
    }

    result = ble_gattc_disc_all_dscs(ble_conn_handle,
                                     start_handle,
                                     end_handle,
                                     ble_on_chr_dsc_discovered,
                                     chr);

    if (result != 0) {
        GG_LOG_WARNING("error starting descriptor discovery");
        ble_on_discovery_done(result);
    }
}

//----------------------------------------------------------------------
static void ble_disc_svc_chrs(struct remote_gatt_svc *svc)
{
    int result;

    GG_LOG_FINEST("Discovering all characteristics start=0x%x, end=0x%x",
                  svc->svc.start_handle,
                  svc->svc.end_handle);
    result = ble_gattc_disc_all_chrs(ble_conn_handle,
                                     svc->svc.start_handle,
                                     svc->svc.end_handle,
                                     ble_on_chr_discovered,
                                     svc);

    if (result != 0) {
        GG_LOG_WARNING("error starting characteristic discovery (0x%x)", result);
        ble_on_discovery_done(result);
    }
}

//----------------------------------------------------------------------
static void ble_do_service_discovery() {
    int result;

    GG_LOG_INFO("Initiating service discovery");
    g_gattc_discovery_failed = false;

    /* Clear remote GATT database */
    remote_gatt_db.num_svc = 0;

    result = ble_gattc_disc_all_svcs(ble_conn_handle,
                                     ble_on_service_discovered,
                                     NULL);
    if (result != 0) {
        GG_LOG_WARNING("ble_gattc_disc_all_svcs failed (%d)", result);
        ble_on_discovery_done(result);
    }
}

/*----------------------------------------------------------------------
|   BLE functions
+---------------------------------------------------------------------*/

//----------------------------------------------------------------------
static void ble_on_reset(int reason)
{
    GG_LOG_WARNING("Resetting state; reason=0x%x", reason);
}

//----------------------------------------------------------------------
static void ble_on_sync(void)
{
    char dev_id_str[HAL_BSP_MAX_ID_LEN * 2 + 1];
    char ble_addr_str[BLE_DEV_ADDR_LEN * 3 + 1];
    uint8_t dev_id[HAL_BSP_MAX_ID_LEN];
    ble_addr_t addr;
    int rc;

    /* Print device id */
    rc = hal_bsp_hw_id(dev_id, HAL_BSP_MAX_ID_LEN);
    if (rc > 0) {
        for (int i = 0; i < rc; i++) {
            sprintf(dev_id_str + i * 2, "%02X", dev_id[i]);
        }

        GG_LOG_INFO("  Device ID: %s", dev_id_str);
    }

    /* Set initial BLE device address. */
    rc = ble_hw_get_static_addr(&addr);
    if (rc != -1) {
        memcpy(g_random_addr, addr.val, BLE_DEV_ADDR_LEN);
    } else {
        GG_LOG_WARNING("Failed to get static BLE addr, falling back to default!");
        memcpy(g_random_addr, ble_dev_addr, BLE_DEV_ADDR_LEN);

        // The two most significant bits of the address must be equal to 1
        g_random_addr[5] |= 0xC0;
        // At least one bit of the random part of the address must be 1
        g_random_addr[5] |= 0x20;
        // At least one bit of the random part of the address must be 0
        g_random_addr[5] &= 0xEF;
    }

    ble_hs_id_set_rnd(g_random_addr);

    for (int i = 0; i < BLE_DEV_ADDR_LEN; i++) {
        sprintf(ble_addr_str + i * 3, "%02X:", g_random_addr[BLE_DEV_ADDR_LEN - 1 - i]);
    }
    ble_addr_str[BLE_DEV_ADDR_LEN * 3 - 1] = '\0';

    GG_LOG_INFO("  BLE address: %s", ble_addr_str);
    GG_LOG_INFO("  Data Length Extension: %d",
                ble_ll_read_supp_features() & BLE_LL_FEAT_DATA_LEN_EXT ? 1 : 0);
    GG_LOG_INFO("  Device name: %s", ble_svc_gap_device_name());

#if MYNEWT_VAL(GG_CONNMGR_CENTRAL)
    rc = nvm_get_peer_addr(&addr);
    if (rc == NVM_OK) {
        GG_ConnMgr_Connect(&addr);
    }
#else
    GG_ConnMgr_AdvertiseEnable();
#endif
}

//----------------------------------------------------------------------
static void ble_on_data_recv_async(void* arg)
{
    GG_DynamicBuffer *buf = (GG_DynamicBuffer *)arg;

    GG_DataSink_PutData(g_connmgr.sink, GG_DynamicBuffer_AsBuffer(buf), NULL);

    GG_DynamicBuffer_Release(buf);
}

//----------------------------------------------------------------------
static void ble_on_data_recv(struct os_mbuf *om)
{
    GG_DynamicBuffer *buf;
    GG_Result rc;
    uint8_t *data;
    size_t len;
    int ret;

    len = OS_MBUF_PKTLEN(om);

    GG_LOG_FINE("Received data size=%d", len);

    if (g_connmgr.sink == NULL) {
        return;
    }

    rc = GG_DynamicBuffer_Create(len, &buf);

    if (rc != GG_SUCCESS) {
        return;
    }

    data = GG_DynamicBuffer_UseData(buf);

    ret = os_mbuf_copydata(om, 0, len, data);

    if (ret != 0) {
        return;
    }

    GG_DynamicBuffer_SetDataSize(buf, len);

    rc = GG_Loop_InvokeAsync(g_connmgr.loop, ble_on_data_recv_async, buf);
    if (rc != 0) {
        GG_LOG_WARNING("Failed to invoke ble_data_recv_async");
    }
}

#if MYNEWT_VAL(GG_CONNMGR_CENTRAL)
//----------------------------------------------------------------------
static void ble_on_link_status_connection_configuration_changed(struct os_mbuf *om)
{
    int len = OS_MBUF_PKTLEN(om);
    if (len < sizeof(GG_LinkStatus_ConnectionConfig)) {
        GG_LOG_WARNING("Received Link Status Connection Configuration with invalid length! (len=%d)", len);
        return;
    }

    GG_LinkStatus_ConnectionConfig* config = OS_MBUF_DATA(om, GG_LinkStatus_ConnectionConfig*);

    GG_LOG_INFO("Received Link Status Connection Configuration:");
    GG_LOG_INFO("    connection_interval: %d.%02d ms", config->connection_interval * 5 / 4, (config->connection_interval * 500 / 4) % 100);
    GG_LOG_INFO("    slave_latency:       %d", config->slave_latency);
    GG_LOG_INFO("    supervision_timeout: %d ms", config->supervision_timeout * 10);
    GG_LOG_INFO("    mtu:                 %d", config->mtu);
    switch(config->mode) {
        case GG_LinkStatus_ConnectionConfigMode_Default:
            GG_LOG_INFO("    mode:                default");
            break;

        case GG_LinkStatus_ConnectionConfigMode_Fast:
            GG_LOG_INFO("    mode:                fast");
            break;

        case GG_LinkStatus_ConnectionConfigMode_Slow:
            GG_LOG_INFO("    mode:                slow");
            break;

        default:
            break;
    }
}

//----------------------------------------------------------------------
static void ble_on_link_status_connection_status_changed(struct os_mbuf *om)
{
    int len = OS_MBUF_PKTLEN(om);
    if (len < sizeof(GG_LinkStatus_ConnectionStatus)) {
        GG_LOG_WARNING("Received Link Status Connection Status with invalid length! (len=%d)", len);
        return;
    }

    GG_LinkStatus_ConnectionStatus* status = OS_MBUF_DATA(om, GG_LinkStatus_ConnectionStatus*);

    GG_LOG_INFO("Received Link Status Connection Status:");
    GG_LOG_INFO("    bonded:              %s", (status->flags & GG_LINK_STATUS_CONNECTION_STATUS_FLAG_HAS_BEEN_BONDED_BEFORE) ? "yes" : "no");
    GG_LOG_INFO("    encrypted:           %s", (status->flags & GG_LINK_STATUS_CONNECTION_STATUS_FLAG_ENCRYPTED) ? "yes" : "no");
    GG_LOG_INFO("    DLE on:              %s", (status->flags & GG_LINK_STATUS_CONNECTION_STATUS_FLAG_DLE_ON) ? "yes" : "no");
    GG_LOG_INFO("    DLE requires reboot: %s", (status->flags & GG_LINK_STATUS_CONNECTION_STATUS_FLAG_DLE_REBOOT_REQUIRED) ? "yes" : "no");
    GG_LOG_INFO("    dle_max_tx_pdu_size: %d", (int)status->dle_max_tx_pdu_size);
    GG_LOG_INFO("    dle_max_tx_time:     %d", (int)status->dle_max_tx_time);
    GG_LOG_INFO("    dle_max_rx_pdu_size: %d", (int)status->dle_max_rx_pdu_size);
    GG_LOG_INFO("    dle_max_rx_time:     %d", (int)status->dle_max_rx_time);
}
#else
//----------------------------------------------------------------------
static void ble_on_link_configuration_connection_configuration_changed(struct os_mbuf *om)
{
    int len = OS_MBUF_PKTLEN(om);
    if (len < sizeof(GG_LinkConfiguration_ConnectionConfig)) {
        GG_LOG_WARNING("Received Connection Configuration with invalid length! (len=%d)", len);
        return;
    }

    GG_LinkConfiguration_ConnectionConfig* config = OS_MBUF_DATA(om, GG_LinkConfiguration_ConnectionConfig*);

    GG_LOG_INFO("Received Connection Configuration:");
    if (config->mask & GG_LINK_CONFIGURATION_CONNECTION_CONFIG_HAS_FAST_MODE_CONFIG) {
        GG_LOG_INFO("  Fast Mode:");
        GG_LOG_INFO("    Min Connection Interval: %d (* 1.25ms)", config->fast_mode_config.min_connection_interval);
        GG_LOG_INFO("    Max Connection Interval: %d (* 1.25ms)", config->fast_mode_config.max_connection_interval);
        GG_LOG_INFO("    Slave Latency:           %d",            config->fast_mode_config.slave_latency);
        GG_LOG_INFO("    Supervision Timeout:     %d (* 10ms)",   config->fast_mode_config.supervision_timeout * 10);
    }
    if (config->mask & GG_LINK_CONFIGURATION_CONNECTION_CONFIG_HAS_SLOW_MODE_CONFIG) {
        GG_LOG_INFO("  Slow Mode:");
        GG_LOG_INFO("    Min Connection Interval: %d (* 1.25ms)", config->slow_mode_config.min_connection_interval);
        GG_LOG_INFO("    Max Connection Interval: %d (* 1.25ms)", config->slow_mode_config.max_connection_interval);
        GG_LOG_INFO("    Slave Latency:           %d",            config->slow_mode_config.slave_latency);
        GG_LOG_INFO("    Supervision Timeout:     %d (* 10ms)",   config->slow_mode_config.supervision_timeout * 10);
    }
    if (config->mask & GG_LINK_CONFIGURATION_CONNECTION_CONFIG_HAS_DLE_CONFIG) {
        GG_LOG_INFO("  DLE:");
        GG_LOG_INFO("    Max TX PDU size:  %d", config->dle_max_tx_pdu_size);
        GG_LOG_INFO("    Max TX time:      %d", config->dle_max_tx_time);
    }
    if (config->mask & GG_LINK_CONFIGURATION_CONNECTION_CONFIG_HAS_MTU) {
        GG_LOG_INFO("  MTU:");
        GG_LOG_INFO("    MTU Size: %d", config->mtu);
    }

    // update the config
    GG_ConnMgr_SetPreferredConnectionConfig(config);
}

//----------------------------------------------------------------------
static void ble_on_link_configuration_connection_mode_changed(struct os_mbuf *om)
{
    int len = OS_MBUF_PKTLEN(om);
    if (len < 1) {
        GG_LOG_WARNING("Received Connection Mode with invalid length! (len=%d)", len);
        return;
    }

    GG_LinkConfiguration_ConnectionMode* mode = OS_MBUF_DATA(om, GG_LinkConfiguration_ConnectionMode*);

    if (mode->speed >= GG_ARRAY_SIZE(speed_str)) {
        GG_LOG_WARNING("Received Preferred Connection Mode with invalid value! (val=%d)", mode->speed);
        return;
    }

    GG_LOG_INFO("Received Preferred Connection Mode:");
    GG_LOG_INFO("    speed: %s", speed_str[mode->speed]);
}
#endif

//----------------------------------------------------------------------
static int ble_gatt_svr_chr_write_cb(uint16_t conn_handle,
                                     uint16_t attr_handle,
                                     struct ble_gatt_access_ctxt *ctxt,
                                     void* arg)
{
    if (attr_handle == gattlink_rx_attr_handle) {
        ble_on_data_recv(ctxt->om);
    } else {
        return BLE_ATT_ERR_UNLIKELY;
    }

    return 0;
}

//----------------------------------------------------------------------
static int ble_gatt_svr_chr_read_cb(uint16_t conn_handle,
                                    uint16_t attr_handle,
                                    struct ble_gatt_access_ctxt *ctxt,
                                    void* arg)
{
#if MYNEWT_VAL(GG_CONNMGR_CENTRAL)
    if (attr_handle == link_configuration_connection_configuration_chr_attr_handle) {
        GG_LOG_INFO("Read on Link Configuration Connection Config characteristic");
        os_mbuf_append(ctxt->om, &g_preferred_conn_config, sizeof(g_preferred_conn_config));
    } else if (attr_handle == link_configuration_connection_mode_chr_attr_handle) {
        GG_LOG_INFO("Read on Link Configuration Connection Mode characteristic");
        os_mbuf_append(ctxt->om, &g_preferred_conn_mode, sizeof(g_preferred_conn_mode));
    }
#endif
#if MYNEWT_VAL(GG_CONNMGR_PERIPHERAL)
    if (attr_handle == link_status_connection_configuration_chr_attr_handle) {
        GG_LOG_INFO("Read on Link Status Connection Configuration characteristic");
        os_mbuf_append(ctxt->om, &g_conn_config, sizeof(g_conn_config));
    } else if (attr_handle == link_status_connection_status_chr_attr_handle) {
        GG_LOG_INFO("Read on Link Status Connection Status characteristic");
        os_mbuf_append(ctxt->om, &g_conn_status, sizeof(g_conn_status));
    } else if (attr_handle == gattlink_l2cap_psm_attr_handle) {
        GG_LOG_INFO("Read on L2CAP PSM characteristic");
        uint16_t psm = GG_GATTLINK_L2CAP_PSM;
        os_mbuf_append(ctxt->om, &psm, sizeof(psm));
    }
#endif

    return 0;
}

//----------------------------------------------------------------------
// Callback invokes when a characteristic is accessed
//----------------------------------------------------------------------
static int ble_gatt_svr_chr_access_cb(uint16_t conn_handle,
                                      uint16_t attr_handle,
                                      struct ble_gatt_access_ctxt *ctxt,
                                      void* arg)
{
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        return ble_gatt_svr_chr_write_cb(conn_handle, attr_handle, ctxt, arg);
    } else if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        return ble_gatt_svr_chr_read_cb(conn_handle, attr_handle, ctxt, arg);
    }

    return BLE_ATT_ERR_UNLIKELY;
}

//----------------------------------------------------------------------
static int gatt_svr_init(void)
{
    int rc;

    rc = ble_gatts_count_cfg(gatt_svr_svcs);
    if (rc != 0) {
        return rc;
    }

    rc = ble_gatts_add_svcs(gatt_svr_svcs);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

//----------------------------------------------------------------------
static void ble_notify_conn_config_update(void)
{
#if MYNEWT_VAL(GG_CONNMGR_PERIPHERAL)
    if (ble_conn_handle == BLE_HS_CONN_HANDLE_NONE) {
        return;
    }

    GG_LOG_INFO("Notifying Link Status connection configuration change");
    int rc = ble_gattc_notify(ble_conn_handle, link_status_connection_configuration_chr_attr_handle);
    if (rc != 0) {
        GG_LOG_WARNING("ble notify link status connection configuration failed (rc=0x%x)", rc);
    }
#endif
}

//----------------------------------------------------------------------
static void ble_on_conn_params_update(void)
{
    struct ble_gap_conn_desc conn_desc;

    ble_gap_conn_find(ble_conn_handle, &conn_desc);

    g_conn_config.connection_interval = conn_desc.conn_itvl;
    g_conn_config.slave_latency       = conn_desc.conn_latency;
    g_conn_config.supervision_timeout = conn_desc.supervision_timeout;

    GG_LOG_INFO("Connection interval is %d.%02d ms",
                g_conn_config.connection_interval * 5 / 4,
                (g_conn_config.connection_interval * 500 / 4) % 100);
    GG_LOG_INFO("Slave latency is %d intervals", g_conn_config.slave_latency);
    GG_LOG_INFO("Supervision timeout is %d ms", g_conn_config.supervision_timeout * 10);
    GG_LOG_INFO("Connection Mode is %s",
                g_conn_config.mode < GG_ARRAY_SIZE(connection_mode_str) ?
                connection_mode_str[g_conn_config.mode] :
                "?");

    ble_notify_conn_config_update();
}

//----------------------------------------------------------------------
static void
ble_l2cap_ready_to_receive(struct ble_l2cap_chan* channel)
{
    struct os_mbuf* sdu_buffer = os_msys_get_pkthdr(GG_GATTLINK_L2CAP_MTU, 0);
    if (sdu_buffer == NULL) {
        GG_LOG_SEVERE("failed to allocated CoC receive buffer");
        return;
    }

    int rc = ble_l2cap_recv_ready(channel, sdu_buffer);
    if (rc != 0) {
        GG_LOG_SEVERE("ble_l2cap_recv_ready failed: %d", rc);
    }
}

//----------------------------------------------------------------------
// Called back when an L2CAP CoC event is received
//----------------------------------------------------------------------
static int ble_on_l2cap_event(struct ble_l2cap_event *event, void *arg)
{
    switch (event->type) {
      case BLE_L2CAP_EVENT_COC_CONNECTED:
        GG_LOG_FINE("BLE_L2CAP_EVENT_COC_CONNECTED: status=%d", event->connect.status);
        if (event->connect.status == 0) {
            // Keep a reference to the channel
            gattlink_l2cap_channel = event->connect.chan;

            // Log the channel info
            struct ble_l2cap_chan_info channel_info;
            int rc = ble_l2cap_get_chan_info(event->connect.chan, &channel_info);
            if (rc == 0) {
                GG_LOG_INFO("L2CAP Channel:");
                GG_LOG_INFO("  source_cid      = %u", channel_info.scid);
                GG_LOG_INFO("  destination_cid = %u", channel_info.dcid);
                GG_LOG_INFO("  our L2CAP MTU   = %u", channel_info.our_l2cap_mtu);
                GG_LOG_INFO("  peer L2CAP MTU  = %u", channel_info.peer_l2cap_mtu);
                GG_LOG_INFO("  PSM             = %u", channel_info.psm);
                GG_LOG_INFO("  our CoC MTU     = %u", channel_info.our_coc_mtu);
                GG_LOG_INFO("  peer CoC MTU    = %u", channel_info.peer_coc_mtu);

            }

            // The link is now up
            GG_LOG_INFO("~~~ Link UP [L2CAP] ~~~");
            g_gattlink_mode = GATTLINK_MODE_L2CAP;
            if (g_client_cbs.mtu_size_change != NULL) {
                g_client_cbs.mtu_size_change(256);
            }
            if (g_client_cbs.connected) {
                g_client_cbs.connected(GG_SUCCESS);
            }
        }
        break;

      case BLE_L2CAP_EVENT_COC_DISCONNECTED:
        GG_LOG_FINE("BLE_L2CAP_EVENT_COC_DISCONNECTED");
        gattlink_l2cap_channel = NULL;
        g_gattlink_mode = GATTLINK_MODE_UNKNOWN;
        break;

      case BLE_L2CAP_EVENT_COC_ACCEPT:
        ble_l2cap_ready_to_receive(event->accept.chan);
        break;

      case BLE_L2CAP_EVENT_COC_DATA_RECEIVED: {
        const struct os_mbuf* data = event->receive.sdu_rx;
        unsigned int data_size = OS_MBUF_PKTLEN(data);
        GG_LOG_FINE("BLE_L2CAP_EVENT_COC_DATA_RECEIVED: %u bytes", data_size);

        // perform packet buffering
        unsigned int offset = 0;
        while (data_size) {
            if (g_gattlink_l2cap_packet_bytes_needed) {
                // copy as much as we can in the packet buffer
                unsigned int chunk = GG_MIN(g_gattlink_l2cap_packet_bytes_needed, data_size);
                os_mbuf_copydata(data, (int)offset, chunk, &g_gattlink_l2cap_packet[g_gattlink_l2cap_packet_size]);
                g_gattlink_l2cap_packet_size += chunk;
                g_gattlink_l2cap_packet_bytes_needed -= chunk;
                data_size -= chunk;
                offset += chunk;

                // if we have completed a packet, emit it now
                if (g_gattlink_l2cap_packet_bytes_needed == 0) {
                    GG_LOG_FINE("Packet complete, size=%u", g_gattlink_l2cap_packet_size);
                    if (g_connmgr.sink) {
                        GG_DynamicBuffer* packet;
                        int result = GG_DynamicBuffer_Create(g_gattlink_l2cap_packet_size, &packet);
                        if (GG_FAILED(result)) {
                            GG_LOG_SEVERE("failed to allocate buffer");
                        } else {
                            GG_DynamicBuffer_SetData(
                                packet,
                                g_gattlink_l2cap_packet,
                                g_gattlink_l2cap_packet_size
                            );
                        }

                        result = GG_Loop_InvokeAsync(g_connmgr.loop, ble_on_data_recv_async, packet);
                        if (result != 0) {
                            GG_LOG_WARNING("Failed to invoke ble_data_recv_async");
                            GG_DynamicBuffer_Release(packet);
                        }

                        // done, reset the buffer
                        g_gattlink_l2cap_packet_size = 0;
                    }
                }
            } else {
                // new packet, with a one byte size header
                uint8_t packet_size_minus_one;
                os_mbuf_copydata(data, (int)offset, 1, &packet_size_minus_one);
                --data_size;
                ++offset;
                g_gattlink_l2cap_packet_bytes_needed = packet_size_minus_one + 1;
            }
        }

        // free the incoming buffer
        os_mbuf_free_chain(event->receive.sdu_rx);

        // we can now receive more
        ble_l2cap_ready_to_receive(event->receive.chan);
        break;
      }

      case BLE_L2CAP_EVENT_COC_TX_UNSTALLED:
        GG_LOG_FINE("BLE_L2CAP_EVENT_COC_TX_UNSTALLED");
        break;

      case BLE_L2CAP_EVENT_COC_RECONFIG_COMPLETED:
        GG_LOG_FINE("BLE_L2CAP_EVENT_COC_RECONFIG_COMPLETED");
        break;

      case BLE_L2CAP_EVENT_COC_PEER_RECONFIGURED:
        GG_LOG_FINE("BLE_L2CAP_EVENT_COC_PEER_RECONFIGURED");
        break;

      default:
          GG_LOG_FINE("L2CAP Unknown Event: %u", event->type);
    }

    return 0;
}

//----------------------------------------------------------------------
// Callback invoked when a connection is established
//----------------------------------------------------------------------
static void ble_gap_event_connect(struct ble_gap_event *event, void *arg)
{
    if (event->connect.status == 0) {
        GG_LOG_INFO("Connection established");

        ble_conn_handle = event->connect.conn_handle;

        g_conn_config.mode = GG_LinkStatus_ConnectionConfigMode_Default;
        g_conn_state = GG_CONNECTION_MANAGER_STATE_CONNECTING;
        g_conn_config.mtu = ble_att_mtu(ble_conn_handle);
        GG_LOG_INFO("MTU is %d", g_conn_config.mtu);

#if MYNEWT_VAL(GG_CONNMGR_CENTRAL)
        int rc = ble_gattc_exchange_mtu(ble_conn_handle, NULL, NULL);
        if (rc != 0) {
            GG_LOG_WARNING("Failed to initiate MTU exchange! (rc=0x%x)", rc);
        }
#endif
        ble_on_conn_params_update();
        ble_do_service_discovery();
    } else {
        if (event->connect.status == BLE_HS_ETIMEOUT) {
            GG_LOG_WARNING("Connection timeout");
        } else {
            GG_LOG_WARNING("Connection failed (status=0x%x)", event->connect.status);
        }

#if MYNEWT_VAL(GG_CONNMGR_CENTRAL) == 0
        /* Connection failed; resume advertising. */
        GG_ConnMgr_AdvertiseEnable();
#endif
    }
}

//----------------------------------------------------------------------
// Callback invoked when a disconnection happens
//----------------------------------------------------------------------
static void ble_gap_event_disconnect(struct ble_gap_event *event, void *arg)
{
    GG_LOG_INFO("Disconnection, reason=0x%x", event->disconnect.reason);

    ble_conn_handle = BLE_HS_CONN_HANDLE_NONE;

    /* Clear remote GATT database */
    remote_gatt_db.num_svc = 0;

    g_conn_state = GG_CONNECTION_MANAGER_STATE_DISCONNECTED;
    if (g_client_cbs.disconnected) {
        g_client_cbs.disconnected();
    }

#if MYNEWT_VAL(GG_CONNMGR_CENTRAL) == 0
    /* Connection terminated. Check advertise on disconnect flag. On true, resume advertising. */
    if (GG_ConnMgr_GetAdvertiseOnDisconnect()) {
        GG_ConnMgr_AdvertiseEnable();
    }
#endif
}

//----------------------------------------------------------------------
// Callback invoked when the MTU has changed
//----------------------------------------------------------------------
static void ble_gap_event_mtu(struct ble_gap_event *event, void *arg)
{
    GG_LOG_INFO("MTU changed to %d", event->mtu.value);

    g_conn_config.mtu = event->mtu.value;

    if (g_gattlink_mode == GATTLINK_MODE_GATT && g_client_cbs.mtu_size_change != NULL) {
        g_client_cbs.mtu_size_change(event->mtu.value);
    }

    ble_notify_conn_config_update();
}

//----------------------------------------------------------------------
// Callback invoked when the connection parameters have changed
//----------------------------------------------------------------------
static void ble_gap_event_conn_update(struct ble_gap_event *event, void *arg)
{
    int err_base;

    if (event->conn_update.status != 0) {
        // Ignore updates caused by disconnect
        err_base = event->conn_update.status & 0xFF;
        if (err_base == BLE_ERR_REM_USER_CONN_TERM || err_base == BLE_ERR_CONN_TERM_LOCAL) {
            return;
        }

        GG_LOG_WARNING("Connection params update failed! (rc=0x%x)", event->conn_update.status);
        return;
    }

    ble_on_conn_params_update();
}

//----------------------------------------------------------------------
// Callback invoked when a peer subscribes to one of our characteristics
//----------------------------------------------------------------------
static void ble_gap_event_subscribe(struct ble_gap_event *event, void *arg)
{
#if MYNEWT_VAL(GG_CONNMGR_CENTRAL)
    if (event->subscribe.attr_handle == link_configuration_connection_configuration_chr_attr_handle) {
        GG_LOG_INFO("%s Link Configuration connection configuration characteristic",
                    event->subscribe.cur_notify ? "Subscription to" : "Unsubscription from");
    } else if (event->subscribe.attr_handle == link_configuration_connection_mode_chr_attr_handle) {
        GG_LOG_INFO("%s Link Configuration connection mode characteristic",
                    event->subscribe.cur_notify ? "Subscription to" : "Unsubscription from");
    } else {
        GG_LOG_INFO("Subscription update for unknown characteristic handle=%d", (int)event->subscribe.attr_handle);
    }
#elif MYNEWT_VAL(GG_CONNMGR_PERIPHERAL)
    if (event->subscribe.attr_handle == link_status_connection_configuration_chr_attr_handle) {
        GG_LOG_INFO("%s Link Status Connection Configuration characteristic",
                    event->subscribe.cur_notify ? "Subscription to" : "Unsubscription from");
    } else if (event->subscribe.attr_handle == link_status_connection_status_chr_attr_handle) {
        GG_LOG_INFO("%s Link Status Connection Status characteristic",
                    event->subscribe.cur_notify ? "Subscription to" : "Unsubscription from");
    } else if (event->subscribe.attr_handle == gattlink_tx_attr_handle) {
        GG_LOG_INFO("%s Gattlink TX characteristic",
                    event->subscribe.cur_notify ? "Subscription to" : "Unsubscription from");
        if (event->subscribe.cur_notify) {
            if (g_conn_state != GG_CONNECTION_MANAGER_STATE_CONNECTED) {
                g_conn_state = GG_CONNECTION_MANAGER_STATE_CONNECTED;
                GG_LOG_INFO("~~~ Link UP [GATT] ~~~");
                g_gattlink_mode = GATTLINK_MODE_GATT;
                if (g_client_cbs.mtu_size_change != NULL) {
                    g_client_cbs.mtu_size_change(g_conn_config.mtu);
                }
                if (g_client_cbs.connected) {
                    g_client_cbs.connected(GG_SUCCESS);
                }
            } else {
                GG_LOG_INFO("Link already up, ignoring subscription");
            }
        } else {
            g_conn_state = GG_CONNECTION_MANAGER_STATE_CONNECTING;
        }
    } else {
        GG_LOG_INFO("Subscription for unknown characteristic handle=%d", (int)event->subscribe.attr_handle);
    }
#endif
}


//----------------------------------------------------------------------
// Callback invoked when a GATT read has completed
//----------------------------------------------------------------------
static int ble_read_cb(struct ble_gatt_attr* attribute)
{
    GG_LOG_FINER("characteristic read completed for handle 0x%x", attribute->handle);

#if MYNEWT_VAL(GG_CONNMGR_PERIPHERAL)
    if (attribute->handle == link_configuration_connection_configuration_chr_attr_handle) {
        ble_on_link_configuration_connection_configuration_changed(attribute->om);
    } else if (attribute->handle == link_configuration_connection_mode_chr_attr_handle) {
        ble_on_link_configuration_connection_mode_changed(attribute->om);
#else
    if (attribute->handle == link_status_connection_configuration_chr_attr_handle) {
        ble_on_link_status_connection_configuration_changed(attribute->om);
    } else if (attribute->handle == link_status_connection_status_chr_attr_handle) {
        ble_on_link_status_connection_status_changed(attribute->om);
#endif
    } else {
        GG_LOG_WARNING("Unexpected GATT read callback for handle 0x%x", attribute->handle);
        return 1;
    }

    return 0;
}

//----------------------------------------------------------------------
// Callback invoked when a GATT subscription has completed
//----------------------------------------------------------------------
static int ble_subscribe_cb(struct ble_gatt_attr* attribute)
{
    GG_LOG_FINER("subscription callback for handle 0x%x", attribute->handle);

#if MYNEWT_VAL(GG_CONNMGR_CENTRAL)
    if (attribute->handle == gattlink_tx_cccd_handle) {
        GG_LOG_FINE("Subscribed to Gattlink TX");
        g_conn_state = GG_CONNECTION_MANAGER_STATE_CONNECTED;
        GG_LOG_INFO("~~~ Link UP ~~~");
        if (g_client_cbs.connected) {
            g_client_cbs.connected(GG_SUCCESS);
        }
    } else if (attribute->handle != link_status_connection_configuration_cccd_handle) {
        // read the characteristic to get its initial value
        GG_LOG_FINE("Subscribed to Link Status connection configuration characteristic");
        ble_gatt_operation operation = {
            .type     = BLE_GATT_OPERATION_READ,
            .handle   = link_status_connection_configuration_chr_attr_handle,
            .callback = ble_read_cb
        };
        ble_queue_gatt_operation(&operation);
    } else if (attribute->handle != link_status_connection_status_cccd_handle) {
        // read the characteristic to get its initial value
        GG_LOG_FINE("Subscribed to Link Status connection configuration characteristic");
        ble_gatt_operation operation = {
            .type     = BLE_GATT_OPERATION_READ,
            .handle   = link_status_connection_status_chr_attr_handle,
            .callback = ble_read_cb
        };
        ble_queue_gatt_operation(&operation);
    } else {
        GG_LOG_WARNING("Subscription callback for unexpected handle 0x%x", attribute->handle);
        return 1;
    }
#else
    if (attribute->handle == link_configuration_connection_configuration_cccd_handle) {
        // read the characteristic to get its initial value
        GG_LOG_FINE("Subscribed to Link Configuration connection configuration characteristic");
        ble_gatt_operation operation = {
            .type     = BLE_GATT_OPERATION_READ,
            .handle   = link_configuration_connection_configuration_chr_attr_handle,
            .callback = ble_read_cb
        };
        ble_queue_gatt_operation(&operation);
    } else if (attribute->handle == link_configuration_connection_mode_cccd_handle) {
        // read the characteristic to get its initial value
        GG_LOG_FINE("Subscribed to Link Configuration connection mode characteristic");
        ble_gatt_operation operation = {
            .type     = BLE_GATT_OPERATION_READ,
            .handle   = link_configuration_connection_mode_chr_attr_handle,
            .callback = ble_read_cb
        };
        ble_queue_gatt_operation(&operation);
    } else if (attribute->handle != link_configuration_connection_configuration_cccd_handle &&
               attribute->handle != link_configuration_connection_mode_cccd_handle) {
        GG_LOG_WARNING("Subscription callback for unexpected handle 0x%x", attribute->handle);
        return 1;
    }
#endif

    return 0;
}

//----------------------------------------------------------------------
// Callback invoked when GATT data is received
//----------------------------------------------------------------------
static void ble_gap_event_notify_rx(struct ble_gap_event *event, void *arg)
{
#if MYNEWT_VAL(GG_CONNMGR_PERIPHERAL)
    if (event->notify_rx.attr_handle == link_configuration_connection_configuration_chr_attr_handle) {
        ble_on_link_configuration_connection_configuration_changed(event->notify_rx.om);
    } else if (event->notify_rx.attr_handle == link_configuration_connection_mode_chr_attr_handle) {
        ble_on_link_configuration_connection_mode_changed(event->notify_rx.om);
    }
#else
    if (event->notify_rx.attr_handle == gattlink_tx_attr_handle) {
        ble_on_data_recv(event->notify_rx.om);
    } else if (event->notify_rx.attr_handle == link_status_connection_configuration_chr_attr_handle) {
        ble_on_link_status_connection_configuration_changed(event->notify_rx.om);
    } else if (event->notify_rx.attr_handle == link_status_connection_status_chr_attr_handle) {
        ble_on_link_status_connection_status_changed(event->notify_rx.om);
    }
#endif
}

//----------------------------------------------------------------------
// Callback invoked when GATT data has been sent
//----------------------------------------------------------------------
static void ble_gap_event_notify_tx(struct ble_gap_event *event, void *arg)
{
    if (event->notify_tx.status != 0) {
        GG_LOG_WARNING("ble TX failed (status=0x%x)", event->notify_tx.status);
    }
}

#if MYNEWT_VAL(GG_CONNMGR_PERIPHERAL) == 0
//----------------------------------------------------------------------
// Callback invoked when a device has been discovered
//----------------------------------------------------------------------
static void ble_gap_event_disc(struct ble_gap_event *event, void *arg)
{
    ble_uuid_any_t uuid;
    uint8_t *ptr;
    uint8_t data_len;
    uint8_t type;
    int rc;

    // Look for a match by address
    if (memcmp(disc_ble_addr, event->disc.addr.val, BLE_DEV_ADDR_LEN)) {
        // Discovery records from new device
        memcpy(disc_ble_addr, event->disc.addr.val, BLE_DEV_ADDR_LEN);

        if (disc_name != NULL) {
            free(disc_name);
            disc_name = NULL;
        }

        disc_uuid_ok = false;
    }

    // Look for a match by advertised name and service ID
    for (int i = 0; i < event->disc.length_data;) {
        // exclude type byte
        data_len = event->disc.data[i] - 1;
        type = event->disc.data[i + 1];
        ptr = &event->disc.data[i + 2];

        switch (type) {
            case BLE_HS_ADV_TYPE_COMP_UUIDS128:
            case BLE_HS_ADV_TYPE_INCOMP_UUIDS128:

                if (ble_uuid_init_from_buf(&uuid, ptr, 16) != 0) {
                    continue;
                }

                // Check for the Gattlink UUID
                if (!ble_uuid_cmp(&uuid.u, &gatt_svr_gattlink_svc_uuid.u)) {
                    disc_uuid_ok = true;
                }
                break;

            case BLE_HS_ADV_TYPE_COMP_NAME:
            case BLE_HS_ADV_TYPE_INCOMP_NAME:

                disc_name = malloc(data_len + 1);
                if (disc_name != NULL) {
                    strncpy(disc_name, (char *)ptr, data_len);
                    disc_name[data_len] = 0;
                }

                break;
        }

        // advance to skip len, type and data bytes
        i += 2 + data_len;
    }

    if (g_peer_name != NULL &&
        (disc_name == NULL || strcmp(disc_name, g_peer_name))) {
        return;
    }

    if (!disc_uuid_ok) {
        return;
    }

    ble_gap_disc_cancel();

    if (disc_name != NULL) {
        GG_LOG_FINE("Connecting to device %s", disc_name);
    }

    // Connect to discovered device
    rc = ble_gap_connect(BLE_OWN_ADDR_RANDOM, &event->disc.addr,
                         BLE_CONNECT_TIMEOUT, NULL, ble_gap_handle_event, NULL);

    if (rc != 0) {
        GG_LOG_WARNING("Failed to init ble connection (rc=0x%x)", rc);

        if (g_client_cbs.connected != NULL) {
            g_client_cbs.connected(GG_FAILURE);
        }
    }

    if (g_peer_name != NULL) {
        free(g_peer_name);
        g_peer_name = NULL;
    }

    if (disc_name != NULL) {
        free(disc_name);
        disc_name = NULL;
    }

    disc_uuid_ok = false;
}
#endif

//----------------------------------------------------------------------
// Top level handler for GAP events
//----------------------------------------------------------------------
static int ble_gap_handle_event(struct ble_gap_event *event, void *arg)
{
    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            GG_LOG_FINE("Received gap event BLE_GAP_EVENT_CONNECT");
            ble_gap_event_connect(event, arg);
            break;

        case BLE_GAP_EVENT_DISCONNECT:
            GG_LOG_FINE("Received gap event BLE_GAP_EVENT_DISCONNECT");
            ble_gap_event_disconnect(event, arg);
            break;

        case BLE_GAP_EVENT_MTU:
            GG_LOG_FINE("Received gap event BLE_GAP_EVENT_MTU");
            ble_gap_event_mtu(event, arg);
            break;

        case BLE_GAP_EVENT_CONN_UPDATE:
            GG_LOG_FINE("Received gap event BLE_GAP_EVENT_CONN_UPDATE");
            ble_gap_event_conn_update(event, arg);
            break;

        case BLE_GAP_EVENT_L2CAP_UPDATE_REQ:
            GG_LOG_FINE("Received gap event BLE_GAP_EVENT_L2CAP_UPDATE_REQ");
            break;

        case BLE_GAP_EVENT_SUBSCRIBE:
            GG_LOG_FINE("Received gap event BLE_GAP_EVENT_SUBSCRIBE");
            ble_gap_event_subscribe(event, arg);
            break;

        case BLE_GAP_EVENT_NOTIFY_RX:
            GG_LOG_FINER("Received gap event BLE_GAP_EVENT_NOTIFY_RX");
            ble_gap_event_notify_rx(event, arg);
            break;

        case BLE_GAP_EVENT_NOTIFY_TX:
            GG_LOG_FINER("Received gap event BLE_GAP_EVENT_NOTIFY_TX");
            ble_gap_event_notify_tx(event, arg);
            break;

#if MYNEWT_VAL(GG_CONNMGR_PERIPHERAL) == 0
        case BLE_GAP_EVENT_DISC:
            // Won't log event type here as scanning will generate a lot of event
            ble_gap_event_disc(event, arg);
            break;

        case BLE_GAP_EVENT_DISC_COMPLETE:
            // BLE scan timeout
            if (g_peer_name != NULL) {
                GG_LOG_WARNING("Failed to discover device \"%s\"!", g_peer_name);

                free(g_peer_name);
                g_peer_name = NULL;
            } else {
                GG_LOG_WARNING("Failed to discover any GG device!");
            }

            if (disc_name != NULL) {
                free(disc_name);
                disc_name = NULL;
            }

            disc_uuid_ok = false;

            if (g_client_cbs.connected != NULL) {
                g_client_cbs.connected(GG_FAILURE);
            }

            break;
#endif

        default:
            GG_LOG_INFO("Received unhandled gap event (%d)", event->type);
            break;
    }

    return 0;
}

/*----------------------------------------------------------------------
|   Connection management logic
+---------------------------------------------------------------------*/

//----------------------------------------------------------------------
GG_Result GG_ConnMgr_Initialize(GG_Loop *loop)
{
    char name[ADV_NAME_MAX_LEN + 1];
    int rc;

    if (loop == NULL) {
        return GG_ERROR_INVALID_PARAMETERS;
    }

    GG_LOG_INFO("Initializing \"%s\"", gap_device_name);

    g_conn_state = GG_CONNECTION_MANAGER_STATE_DISCONNECTED;
    g_connmgr.loop = loop;
    g_connmgr.listener = NULL;
    g_connmgr.sink = NULL;

    GG_SET_INTERFACE(&g_connmgr, GG_ConnMgr, GG_DataSink);
    GG_SET_INTERFACE(&g_connmgr, GG_ConnMgr, GG_DataSource);

    /* Initialize the NimBLE host configuration. */
    ble_hs_cfg.reset_cb = ble_on_reset;
    ble_hs_cfg.sync_cb  = ble_on_sync;

    /* Initialize the GATT server */
    rc = gatt_svr_init();
    if (rc != 0) {
        return GG_FAILURE;
    }

    /* Create an L2CAP server */
    rc = ble_l2cap_create_server(
        GG_GATTLINK_L2CAP_PSM,
        GG_GATTLINK_L2CAP_MTU,
        ble_on_l2cap_event,
        NULL
    );
    if (rc != 0) {
        GG_LOG_SEVERE("ble_l2cap_create_server failed: %d", rc);
    }

    /* Set the default device name. */
    rc = nvm_get_adv_name(name, sizeof(name));
    if (rc == 0) {
        rc = ble_svc_gap_device_name_set(name);
    } else {
        rc = ble_svc_gap_device_name_set(gap_device_name);
    }

    // init the subscription queue
    os_mutex_init(&g_ble_gatt_operation_queue.mutex);

    return rc == 0 ? GG_SUCCESS : GG_FAILURE;
}

//----------------------------------------------------------------------
GG_ConnMgrState
GG_ConnMgr_GetState(void)
{
    return g_conn_state;
}

//----------------------------------------------------------------------
GG_Result GG_ConnMgr_SetAdvertiseName(const char *name)
{
    int is_advertising;
    GG_Result rc_name;
    GG_Result rc = GG_SUCCESS;

    is_advertising = ble_gap_adv_active();

    if (is_advertising) {
        rc = GG_ConnMgr_AdvertiseDisable();

        if (rc != GG_SUCCESS) {
            return rc;
        }
    }

    rc_name = ble_svc_gap_device_name_set(name);

    if (is_advertising) {
        rc = GG_ConnMgr_AdvertiseEnable();
    }

    if (rc_name != GG_SUCCESS) {
        GG_LOG_WARNING("Failed to set advertise name!");
        return GG_FAILURE;
    }

    return rc;
}

//----------------------------------------------------------------------
GG_Result GG_ConnMgr_AdvertiseEnable(void)
{
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields fields;
    const char *name;
    int rc;

#if MYNEWT_VAL(GG_CONNMGR_CENTRAL)
    return GG_ERROR_NOT_SUPPORTED;
#endif

    /* basic advertising data */
    memset(&fields, 0, sizeof fields);

    fields.flags                 = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;

    fields.tx_pwr_lvl_is_present = 1;
    fields.tx_pwr_lvl            = BLE_HS_ADV_TX_PWR_LVL_AUTO;

    fields.uuids128              = (ble_uuid128_t*)gatt_svr_svcs[0].uuid;
    fields.num_uuids128          = 1;
    fields.uuids128_is_complete  = 1;

    rc = ble_gap_adv_set_fields(&fields);

    if (rc == BLE_HS_EBUSY) {
        // advertising enabled, updates not allowed
        // this is used liberally during testing and shouldn't result in an error/failure
        GG_LOG_WARNING("advertise enable ignored, advertising already enabled; rc=0x%x", rc);
        return GG_SUCCESS;
    }

    if (rc != 0) {
        GG_LOG_SEVERE("error setting basic advertisement data; rc=0x%x", rc);
        return GG_FAILURE;
    }

    /* extended advertising data */
    memset(&fields, 0, sizeof fields);

    name                    = ble_svc_gap_device_name();
    fields.name             = (uint8_t *)name;
    fields.name_len         = strlen(name);
    fields.name_is_complete = 1;

    rc = ble_gap_adv_rsp_set_fields(&fields);
    if (rc != 0) {
        GG_LOG_SEVERE("error setting extended advertisement data; rc=0x%x", rc);
        return GG_FAILURE;
    }

    /* start advertising. */
    memset(&adv_params, 0, sizeof adv_params);
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    rc = ble_gap_adv_start(BLE_OWN_ADDR_RANDOM, NULL, BLE_HS_FOREVER,
                           &adv_params, ble_gap_handle_event, NULL);

    if (rc != 0) {
        GG_LOG_SEVERE("error enabling advertising; rc=0x%x", rc);
        return GG_FAILURE;
    }

#if MYNEWT_VAL(GG_CONNMGR_PERIPHERAL)
    /* Reset advertise on disconnect flag to TRUE (default behavior) */
    GG_ConnMgr_SetAdvertiseOnDisconnect(true);
#endif
    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_Result GG_ConnMgr_AdvertiseDisable(void)
{
    int rc;

#if MYNEWT_VAL(GG_CONNMGR_CENTRAL)
    return GG_ERROR_NOT_SUPPORTED;
#endif

    GG_ConnMgr_SetAdvertiseOnDisconnect(true);

    rc = ble_gap_adv_stop();
    if (rc != 0) {
        GG_LOG_SEVERE("error disabling advertisement; rc=0x%x", rc);
        return GG_FAILURE;
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
void GG_ConnMgr_RegisterClient(GG_ConnMgr_Client_Callback_Functions *cbs)
{
    if (cbs == NULL) {
        memset(&g_client_cbs, 0, sizeof(g_client_cbs));
    } else {
        g_client_cbs = *cbs;
    }
}

//----------------------------------------------------------------------
uint16_t GG_ConnMgr_GetMTUSize(void)
{
    return ble_att_mtu(ble_conn_handle);
}

//----------------------------------------------------------------------
GG_Result GG_ConnMgr_ChangeMTUSize(uint16_t mtu)
{
    int rc;

    rc = ble_att_set_preferred_mtu(mtu);
    if (rc != 0) {
        GG_LOG_WARNING("Failed for set preferred MTU! (rc=0x%x)", rc);
        return GG_ERROR_INVALID_PARAMETERS;
    }

    if (GG_ConnMgr_GetState() != GG_CONNECTION_MANAGER_STATE_CONNECTED) {
        return GG_SUCCESS;
    }

    rc = ble_gattc_exchange_mtu(ble_conn_handle, NULL, NULL);
    if (rc == BLE_HS_EALREADY) {
        GG_LOG_WARNING("MTU exchange has already been done!");
        return GG_ERROR_INVALID_STATE;
    } else if (rc != 0) {
        GG_LOG_WARNING("Failed to send MTU exchange request! (rc=0x%x)", rc);
        return GG_FAILURE;
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_Result GG_ConnMgr_ChangeConnectionConfig(const GG_LinkConfiguration_ConnectionModeConfig* config)
{
    struct ble_gap_upd_params params;
    int rc;

    params.itvl_min            = config->min_connection_interval;
    params.itvl_max            = config->max_connection_interval;
    params.latency             = config->slave_latency;
    params.supervision_timeout = config->supervision_timeout * 10; // converted from 100ms to 10ms units
    params.min_ce_len          = 0x10;
    params.max_ce_len          = 0x300;

    GG_LOG_INFO("Requesting connection parameters update: itvl_min=%u, itvl_max=%u, latency=%u, timeout=%u",
                params.itvl_min,
                params.itvl_max,
                params.latency ,
                params.supervision_timeout);
    rc = ble_gap_update_params(ble_conn_handle, &params);
    if (rc != 0) {
        GG_LOG_WARNING("Failed to update connection parameters! (rc=0x%x)", rc);
        return GG_FAILURE;
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_Result GG_ConnMgr_SetPreferredConnectionConfig(const GG_LinkConfiguration_ConnectionConfig* config)
{
    if (config->mask & GG_LINK_CONFIGURATION_CONNECTION_CONFIG_HAS_FAST_MODE_CONFIG) {
        g_preferred_conn_config.fast_mode_config = config->fast_mode_config;
    }
    if (config->mask & GG_LINK_CONFIGURATION_CONNECTION_CONFIG_HAS_SLOW_MODE_CONFIG) {
        g_preferred_conn_config.slow_mode_config = config->slow_mode_config;
    }
    if (config->mask & GG_LINK_CONFIGURATION_CONNECTION_CONFIG_HAS_DLE_CONFIG) {
        g_preferred_conn_config.dle_max_tx_pdu_size = config->dle_max_tx_pdu_size;
        g_preferred_conn_config.dle_max_tx_time     = config->dle_max_tx_time;
    }
    if (config->mask & GG_LINK_CONFIGURATION_CONNECTION_CONFIG_HAS_MTU) {
        g_preferred_conn_config.mtu = config->mtu;
    }
#if MYNEWT_VAL(GG_CONNMGR_CENTRAL)
    // notify of the new value (we notify even if nothing has changed, for simplicity)
    if (ble_conn_handle != BLE_HS_CONN_HANDLE_NONE) {
        GG_LOG_INFO("Notifying Link Configuration connection configuration change");
        int rc = ble_gattc_notify(ble_conn_handle, link_configuration_connection_configuration_chr_attr_handle);
        if (rc != 0) {
            GG_LOG_WARNING("Failed to notify preferred connection configuration!");
            return GG_FAILURE;
        }
    }
#endif
    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_Result GG_ConnMgr_ChangeConnectionSpeed(GG_LinkConfiguration_ConnectionSpeed speed)
{
#if MYNEWT_VAL(GG_CONNMGR_CENTRAL)
    if ((uint8_t)speed != g_preferred_conn_mode.speed) {
        // the config has changed, notify
        g_preferred_conn_mode.speed = (uint8_t)speed;

        if (ble_conn_handle != BLE_HS_CONN_HANDLE_NONE) {
            GG_LOG_INFO("Notifying Link Configuration connection mode change");
            int rc = ble_gattc_notify(ble_conn_handle, link_configuration_connection_mode_chr_attr_handle);
            if (rc != 0) {
                GG_LOG_WARNING("Failed to notify connection mode!");
                return GG_FAILURE;
            }
        }
    }
    return GG_SUCCESS;
#else
    switch (speed) {
        case GG_LinkConfiguration_ConnectionSpeed_Fast: {
            g_conn_config.mode = GG_LinkStatus_ConnectionConfigMode_Fast;
            // NOTE: we make a local copy here to avoid taking the address of a packed struct
            GG_LinkConfiguration_ConnectionModeConfig mode_config = g_preferred_conn_config.fast_mode_config;
            return GG_ConnMgr_ChangeConnectionConfig(&mode_config);
        }

        case GG_LinkConfiguration_ConnectionSpeed_Slow: {
            g_conn_config.mode = GG_LinkStatus_ConnectionConfigMode_Slow;
            // NOTE: we make a local copy here to avoid taking the address of a packed struct
            GG_LinkConfiguration_ConnectionModeConfig mode_config = g_preferred_conn_config.slow_mode_config;
            return GG_ConnMgr_ChangeConnectionConfig(&mode_config);
        }
    }

    return GG_ERROR_INVALID_PARAMETERS;
#endif
}

//----------------------------------------------------------------------
GG_Result GG_ConnMgr_Connect(const ble_addr_t *addr)
{
#if MYNEWT_VAL(GG_CONNMGR_PERIPHERAL)
    return GG_ERROR_NOT_SUPPORTED;
#else
    int rc;

    if (ble_conn_handle != BLE_HS_CONN_HANDLE_NONE) {
        GG_LOG_WARNING("Already connected to a GG peer!");
        return GG_ERROR_INVALID_STATE;
    }

    rc = ble_gap_connect(BLE_OWN_ADDR_RANDOM, addr, BLE_CONNECT_TIMEOUT,
                         NULL, ble_gap_handle_event, NULL);

    if (rc != 0) {
        GG_LOG_WARNING("Failed to init ble connection (rc=0x%x)", rc);
        return GG_FAILURE;
    }

    return GG_SUCCESS;
#endif
}

//----------------------------------------------------------------------
GG_Result GG_ConnMgr_ScanAndConnect(const char *peer_name)
{
#if MYNEWT_VAL(GG_CONNMGR_PERIPHERAL)
    return GG_ERROR_NOT_SUPPORTED;
#else
    struct ble_gap_disc_params params;
    int rc;

    if (ble_conn_handle != BLE_HS_CONN_HANDLE_NONE) {
        GG_LOG_WARNING("Already connected to a GG peer!");
        return GG_ERROR_INVALID_STATE;
    }

    if (peer_name) {
        g_peer_name = strdup(peer_name);
    } else {
        g_peer_name = NULL;
    }

    memset(&params, 0, sizeof(params));

    rc = ble_gap_disc(BLE_OWN_ADDR_RANDOM, BLE_SCAN_TIMEOUT,
                      &params, ble_gap_handle_event, NULL);

    if (rc != 0) {
        GG_LOG_WARNING("Failed to start ble scanning (rc=0x%x)", rc);
        return GG_FAILURE;
    }

    GG_LOG_INFO("BLE scan started");

    return GG_SUCCESS;
#endif
}

//----------------------------------------------------------------------
GG_Result GG_ConnMgr_Disconnect(void)
{
    int rc;

    if (ble_conn_handle == BLE_HS_CONN_HANDLE_NONE) {
        GG_LOG_WARNING("Not connected to a GG peer!");
        return GG_ERROR_INVALID_STATE;
    }

    rc = ble_gap_terminate(ble_conn_handle, BLE_ERR_REM_USER_CONN_TERM);

    if (rc != 0) {
        GG_LOG_WARNING("Failed to disconnect from GG peer (rc=0x%x)", rc);
        return GG_FAILURE;
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_LinkStatus_ConnectionStatus* GG_ConnMgr_GetConnStatus(void)
{
    return &g_conn_status;
}

//----------------------------------------------------------------------
GG_LinkStatus_ConnectionConfig* GG_ConnMgr_GetConnConfig(void)
{
    return &g_conn_config;
}

//----------------------------------------------------------------------
GG_DataSink* GG_ConnMgr_AsDataSink(void)
{
    return GG_CAST(&g_connmgr, GG_DataSink);
}

//----------------------------------------------------------------------
GG_DataSource* GG_ConnMgr_AsDataSource(void)
{
    return GG_CAST(&g_connmgr, GG_DataSource);
}


//----------------------------------------------------------------------
void GG_ConnMgr_SetAdvertiseOnDisconnect(bool advertise) {
    g_connmgr_advertise_on_disconnect = advertise;
}

//----------------------------------------------------------------------
bool GG_ConnMgr_GetAdvertiseOnDisconnect(void) {
    return g_connmgr_advertise_on_disconnect;
}

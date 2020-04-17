/**
 * @file
 * @brief Golden Gate Connection Manager API
 *
 * @copyright
 * Copyright 2017-2020 Fitbit, Inc
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author Bogdan Davidoaia
 *
 * @date 2017-11-13
 *
 * @details
 * Detailed information can be found at this link:
 *  https://wiki.fitbit.com/display/firmware/BLE+Connection+Manager
 *
 */

#ifndef __CONNECTION_MANAGER_H__
#define __CONNECTION_MANAGER_H__

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <inttypes.h>

#include "xp/common/gg_io.h"
#include "xp/common/gg_results.h"
#include "xp/loop/gg_loop.h"

/*----------------------------------------------------------------------
|   Connection Manager type check
+---------------------------------------------------------------------*/

#if (MYNEWT_VAL(GG_CONNMGR_CENTRAL) + MYNEWT_VAL(GG_CONNMGR_PERIPHERAL)) == 0
#error "A Connection Manager Type needs to be enabled!"
#endif

#if (MYNEWT_VAL(GG_CONNMGR_CENTRAL) + MYNEWT_VAL(GG_CONNMGR_PERIPHERAL)) > 1
#error "Enabling only one Connection Manager Type is allowed!"
#endif

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/

/**
 * Enum for the Link Configuration Service's connection mode values
 */
typedef enum {
    GG_LinkConfiguration_ConnectionSpeed_Fast = 0,
    GG_LinkConfiguration_ConnectionSpeed_Slow
} GG_LinkConfiguration_ConnectionSpeed;

/**
 * Structure representing a mode configuration
 */
typedef struct {
    uint16_t min_connection_interval; // units of 1.25ms
    uint16_t max_connection_interval; // units of 1.25ms
    uint8_t  slave_latency;           // in number of connection events
    uint8_t  supervision_timeout;     // in units of 100ms
} GG_LinkConfiguration_ConnectionModeConfig;

/**
  * Structure representing a Link Configuration preferred connection config.
  */
typedef struct __attribute__((packed)) {
    uint8_t  mask; // mask to indicate what fields are set
    GG_LinkConfiguration_ConnectionModeConfig fast_mode_config;
    GG_LinkConfiguration_ConnectionModeConfig slow_mode_config;
    uint8_t                                   dle_max_tx_pdu_size;
    uint16_t                                  dle_max_tx_time;
    uint16_t                                  mtu;
} GG_LinkConfiguration_ConnectionConfig;

/**
  * Structure representing a Link Configuration preferred connection mode.
  */
typedef struct __attribute__((packed)) {
    uint8_t speed;
} GG_LinkConfiguration_ConnectionMode;

/**
 * Flags used with GG_LinkConfiguration_ConnectionConfig
 */
#define GG_LINK_CONFIGURATION_CONNECTION_CONFIG_HAS_FAST_MODE_CONFIG 1
#define GG_LINK_CONFIGURATION_CONNECTION_CONFIG_HAS_SLOW_MODE_CONFIG 2
#define GG_LINK_CONFIGURATION_CONNECTION_CONFIG_HAS_DLE_CONFIG       4
#define GG_LINK_CONFIGURATION_CONNECTION_CONFIG_HAS_MTU              8

/**
 * Enum for the Link Status Service connection mode values
 */
typedef enum {
    GG_LinkStatus_ConnectionConfigMode_Default = 0,
    GG_LinkStatus_ConnectionConfigMode_Fast = 1,
    GG_LinkStatus_ConnectionConfigMode_Slow = 2
} GG_LinkStatus_ConnectionConfigMode;

/**
  * Structure representing a Link Status connection configuration
  */
typedef struct __attribute__((packed)) {
    uint16_t  connection_interval; // in units of 1.25ms
    uint16_t  slave_latency;       // in number of connection events
    uint16_t  supervision_timeout; // in units of 10ms
    uint16_t  mtu;                 // in bytes
    uint8_t   mode;                // see GG_LinkStatus_ConnectionConfigMode
} GG_LinkStatus_ConnectionConfig;

/**
  * Structure representing a Link Status connection status
  */
typedef struct __attribute__((packed)) {
    uint8_t  flags; // see below
    uint8_t  dle_max_tx_pdu_size;
    uint16_t dle_max_tx_time;
    uint8_t  dle_max_rx_pdu_size;
    uint16_t dle_max_rx_time;
} GG_LinkStatus_ConnectionStatus;

/**
 * Flags for GG_LinkStatus_ConnectionStatus
 */
#define GG_LINK_STATUS_CONNECTION_STATUS_FLAG_HAS_BEEN_BONDED_BEFORE 1
#define GG_LINK_STATUS_CONNECTION_STATUS_FLAG_ENCRYPTED              2
#define GG_LINK_STATUS_CONNECTION_STATUS_FLAG_DLE_ON                 4
#define GG_LINK_STATUS_CONNECTION_STATUS_FLAG_DLE_REBOOT_REQUIRED    8

/**
 * Function prototype for client callback to indicate the result of the
 * call to GG_ConnMgr_Connect.
 *
 * @param status GG_SUCCESS on connection established, or a negative number
 * on connection failure
 */
typedef void (*GG_ConnMgr_Client_Connected)(GG_Result status);

/**
 * Function prototype for client callback to indicate that a GG Connection
 * has been lost/disconnected.
 */
typedef void (*GG_ConnMgr_Client_Disconnected)(void);

/**
 * Function prototype for client callback to indicate a change in MTU.
 *
 * @param size New value of MTU size
 */
typedef void (*GG_ConnMgr_Client_MTUSizeChange)(uint16_t size);

/**
 * Function prototype for client callback to indicate a change in the GG
 * Connection's speed.
 *
 * @param speed New value of GG Connection speed
 */
typedef void (*GG_ConnMgr_Client_ConnectionSpeedChange)(GG_LinkConfiguration_ConnectionSpeed speed);

/**
 * Structure containing client callback functions used to indicate different
 * GG Connection Manager events.
 */
typedef struct {
    GG_ConnMgr_Client_Connected             connected;
    GG_ConnMgr_Client_Disconnected          disconnected;
    GG_ConnMgr_Client_MTUSizeChange         mtu_size_change;
    GG_ConnMgr_Client_ConnectionSpeedChange connection_speed_changed;
} GG_ConnMgr_Client_Callback_Functions;

/**
 * GG Connection Manager State
 */
typedef enum {
    GG_CONNECTION_MANAGER_STATE_DISCONNECTED,  // No BLE connection
    GG_CONNECTION_MANAGER_STATE_CONNECTING,    // BLE connection, but Gattlink not setup yet
    GG_CONNECTION_MANAGER_STATE_CONNECTED      // Gattlink setup
} GG_ConnMgrState;

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

/**
 * Initialize GG Connection Manager
 *
 * @param loop Pointer to GG_Loop on which to process received BLE data
 * @retrun GG_SUCCESS on success, or negative number on failure
 */
GG_Result GG_ConnMgr_Initialize(GG_Loop *loop);

/**
 * Set BLE advertising device name
 *
 * @param name Device name to be used in advertising
 * @return GG_SUCCESS on success, or negative number on failure
 */
GG_Result GG_ConnMgr_SetAdvertiseName(const char *name);

/**
 * Start BLE advertising
 *
 * @return GG_SUCCESS on success, or negative number on failure
 */
GG_Result GG_ConnMgr_AdvertiseEnable(void);

/**
 * Start BLE advertising
 *
 * @return GG_SUCCESS on success, or negative number on failure
 */
GG_Result GG_ConnMgr_AdvertiseDisable(void);

/**
 * Stop BLE advertising
 *
 * @return GG_SUCCESS on success, or negative number on failure
 */
void GG_ConnMgr_RegisterClient(GG_ConnMgr_Client_Callback_Functions *cbs);

/**
 * Get MTU size for current GG Connection
 *
 * @return MTU size
 */
uint16_t GG_ConnMgr_GetMTUSize(void);

/**
 * Sets the preferred connection config that a central would like a peripheral to adopt.
 * @note The return value only indicates the success/failure of the request.
 *
 * @param req Pointer to a Connection Config structure.
 * @return GG_SUCCESS on success, or negative number on failure
 */
GG_Result GG_ConnMgr_SetPreferredConnectionConfig(const GG_LinkConfiguration_ConnectionConfig *config);

/**
 * Request changing MTU size for current GG Connection
 * @note The return value only indicates the success/failure of the request.
 * A call to the registered GG_ConnMgr_Client_MTUSizeChange callback
 *
 * @param mtu Value of MTU to be set
 * @return GG_SUCCESS on success, or negative number on failure
 */
GG_Result GG_ConnMgr_ChangeMTUSize(uint16_t mtu);

/**
 * Request change of connection parameters for current GG Connection
 * @note The return value only indicates the success/failure of the request.
 *
 * @params config Pointer to config change request object
 * @return GG_SUCCESS on success, or negative number on failure
 */
GG_Result GG_ConnMgr_ChangeConnectionConfig(const GG_LinkConfiguration_ConnectionModeConfig *config);

/**
 * Request change of current GG Connection speed
 * @note The return value only indicates the success/failure of the request.
 * A call to the registered GG_ConnMgr_Client_ConnectionSpeedChange callback
 * will indicate if the request succeeded/failed.
 *
 * @param speed Requested speed
 * @return GG_SUCCESS on success, or negative number on failure
 */
GG_Result GG_ConnMgr_ChangeConnectionSpeed(GG_LinkConfiguration_ConnectionSpeed speed);

/**
 * Request establishment of a GG Connection with a peer device
 * @note The return value only indicates the success/failure of the request.
 * A call to the registered GG_ConnMgr_Client_Connected callback will indicate
 * if the request succeeded/failed.
 *
 * @param addr BLE address of peer to which to connect
 * @return GG_SUCCESS on success, or negative number on failure
 */
GG_Result GG_ConnMgr_Connect(const ble_addr_t *mac_addr);

/**
 * Request scan for nearby GG peers and establishment of a GG Connection
 * @note If the peer_name param is not NULL, it will connect to the first
 * found GG device advertising that name; if it is NULL, connect to the first
 * found GG device regardless of name
 * @note The return value only indicates the success/failure of the request.
 * A call to the registered GG_ConnMgr_Client_Connected callback will indicate
 * if the request succeeded/failed.
 *
 * @param peer_name Advertised name of peer to which to connect
 * @return GG_SUCCESS on success, or negative number on failure
 */
GG_Result GG_ConnMgr_ScanAndConnect(const char *peer_name);

/**
 * Request current GG Connection to be disconnected.
 * @note The return value only indicates the success/failure of the request.
 * A call to the registered GG_ConnMgr_Client_Disconnected callback will
 * indicate if the request succeeded/failed.
 *
 * @return GG_SUCCESS on success, or negative number on failure
 */
GG_Result GG_ConnMgr_Disconnect(void);

/**
 * Get the state of the Connection Manager
 *
 * @return Current state of the GG Connection Manager
 */
GG_ConnMgrState GG_ConnMgr_GetState(void);

/**
 * Get the GG Connection Manager connection status
 *
 * @return reference to GG_Connection_Status object
 */
GG_LinkStatus_ConnectionStatus* GG_ConnMgr_GetConnStatus(void);

/**
 * Get the GG Connection Manager connection configuration
 *
 * @return reference to GG_Connection_Config object
 */
GG_LinkStatus_ConnectionConfig* GG_ConnMgr_GetConnConfig(void);

/**
 * Get reference to GG_DataSink object for sending data to peer
 *
 * @return Pointer to GG_DataSink object
 */
GG_DataSink* GG_ConnMgr_AsDataSink(void);

/**
 * Get reference to GG_DataSource object for receiving data from peer
 *
 * @return Pointer to GG_DataSource object
 */
GG_DataSource* GG_ConnMgr_AsDataSource(void);

/**
 * Set a flag that changes the behavior on disconnect with regards to advertising.
 *
 * @param advertise: if TRUE, advertise after disconnect completed
 * @notes: Default/Boot value is TRUE. Flag is reset to TRUE after enabling advertising
 * Command applies only for peripheral devices.
 */
void GG_ConnMgr_SetAdvertiseOnDisconnect(bool advertise);

/**
 * Get the flag that determines the behavior on disconnect with regards to advertising
 *
 * @note Query applies only on peripheral devices.
 * @return value of flag. If true, advertise after completing disconnect.
 */
bool GG_ConnMgr_GetAdvertiseOnDisconnect(void);

#endif /* __CONNECTION_MANAGER_H__ */

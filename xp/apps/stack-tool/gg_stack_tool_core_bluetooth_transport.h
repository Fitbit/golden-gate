/**
 *
 * @file
 *
 * @copyright
 * Copyright 2017-2020 Fitbit, Inc
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author Gilles Boccon-Gibod
 *
 * @date 2018-12-26
 *
 * @details
 *
 * Core Bluetooth transport interface
 */

//----------------------------------------------------------------------
// includes
//----------------------------------------------------------------------
#include "xp/common/gg_common.h"
#include "xp/loop/gg_loop.h"

//----------------------------------------------------------------------
// types
//----------------------------------------------------------------------
typedef struct GG_StackToolBluetoothTransport GG_StackToolBluetoothTransport;

typedef struct {
    GG_Event    base;
    const char* peripheral_name;
    const char* peripheral_id;
    int         rssi;
} GG_StackToolBluetoothTransportScanEvent;

typedef struct {
    GG_Event     base;
    unsigned int connection_interval;
    unsigned int slave_latency;
    unsigned int supervision_timeout;
    unsigned int mtu;
    unsigned int mode;
} GG_StackToolBluetoothTransportLinkStatusConnectionConfigEvent;

//----------------------------------------------------------------------
// constants
//----------------------------------------------------------------------
#define GG_EVENT_TYPE_BLUETOOTH_TRANSPORT_SCAN                      GG_4CC('s', 'c', 'a', 'n')
#define GG_EVENT_TYPE_BLUETOOTH_LINK_CONNECTED_EVENT                GG_4CC('l', 'i', 'n', 'k')
#define GG_EVENT_TYPE_BLUETOOTH_LINK_STATUS_CONENCTION_CONFIG_EVENT GG_4CC('l', 's', 'c', 'c')

//----------------------------------------------------------------------
// functions
//----------------------------------------------------------------------
GG_Result GG_StackToolBluetoothTransport_Create(GG_Loop*                         loop,
                                                const char*                      device_id,
                                                GG_StackToolBluetoothTransport** transport);
void GG_StackToolBluetoothTransport_Destroy(GG_StackToolBluetoothTransport* self);
void GG_StackToolBluetoothTransport_Start(GG_StackToolBluetoothTransport* self);
void GG_StackToolBluetoothTransport_Connect(GG_StackToolBluetoothTransport* self, const char* device_id);
void GG_StackToolBluetoothTransport_SetPreferredConnectionMode(GG_StackToolBluetoothTransport* self, uint8_t mode);
GG_DataSource* GG_StackToolBluetoothTransport_AsDataSource(GG_StackToolBluetoothTransport* self);
GG_DataSink* GG_StackToolBluetoothTransport_AsDataSink(GG_StackToolBluetoothTransport* self);
void GG_StackToolBluetoothTransport_SetMtuListener(GG_StackToolBluetoothTransport* self, GG_EventListener* listener);
void GG_StackToolBluetoothTransport_SetScanListener(GG_StackToolBluetoothTransport* self, GG_EventListener* listener);
void GG_StackToolBluetoothTransport_SetConnectionListener(GG_StackToolBluetoothTransport* self,
                                                          GG_EventListener*               listener);

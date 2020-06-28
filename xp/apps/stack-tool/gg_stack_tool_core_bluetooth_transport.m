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
// imports and includes
//----------------------------------------------------------------------
#include <stdio.h>
#import <CoreBluetooth/CoreBluetooth.h>

#include "xp/common/gg_common.h"
#include "xp/stack_builder/gg_stack_builder.h"
#include "gg_stack_tool_core_bluetooth_transport.h"

//----------------------------------------------------------------------
// constants
//----------------------------------------------------------------------

// Link Status Service
#define GG_LINK_STATUS_SERVICE_UUID                                        @"ABBAFD00-E56A-484C-B832-8B17CF6CBFE8"
#define GG_LINK_STATUS_CONNECTION_CONFIGURATION_CHARACTERISTIC_UUID        @"ABBAFD01-E56A-484C-B832-8B17CF6CBFE8"
#define GG_LINK_STATUS_CONNECTION_STATUS_CHARACTERISTIC_UUID               @"ABBAFD02-E56A-484C-B832-8B17CF6CBFE8"
#define GG_LINK_STATUS_SECURE_CHARACTERISTIC_UUID                          @"ABBAFD03-E56A-484C-B832-8B17CF6CBFE8"

// Gattlink Service
#define GG_GATTLINK_SERVICE_UUID                                           @"ABBAFF00-E56A-484C-B832-8B17CF6CBFE8"
#define GG_GATTLINK_RX_CHARACTERISTIC_UUID                                 @"ABBAFF01-E56A-484C-B832-8B17CF6CBFE8"
#define GG_GATTLINK_TX_CHARACTERISTIC_UUID                                 @"ABBAFF02-E56A-484C-B832-8B17CF6CBFE8"

// Link Configuration Service
#define GG_LINK_CONFIGURATION_SERVICE_UUID                                 @"ABBAFC00-E56A-484C-B832-8B17CF6CBFE8"
#define GG_LINK_CONFIGURATION_CONNECTION_CONFIGURATION_CHARACTERISTIC_UUID @"ABBAFC01-E56A-484C-B832-8B17CF6CBFE8"
#define GG_LINK_CONFIGURATION_CONNECTION_MODE_CHARACTERISTIC_UUID          @"ABBAFC02-E56A-484C-B832-8B17CF6CBFE8"
#define GG_LINK_CONFIGURATION_GENERAL_PURPOSE_COMMAND_CHARACTERISTIC_UUID  @"ABBAFC03-E56A-484C-B832-8B17CF6CBFE8"

// GATT Confirmation Service
#define GG_GATT_CONFIRMATION_SERVICE_UUID                                  @"AC2F0045-8182-4BE5-91E0-2992E6B40EBB"
#define GG_GATT_CONFIRMATION_EPHEMERAL_CHARACTERISTIC_POINTER_UUID         @"AC2F0145-8182-4BE5-91E0-2992E6B40EBB"

#define GG_STACK_TOOL_RX_QUEUE_SIZE  32
#define GG_STACK_TOOL_TX_QUEUE_SIZE  32
#define GG_STACK_TOOL_TX_DEFAULT_MTU 20
#define GG_STACK_TOOL_MAX_MTU        512

#define GG_LINK_STATUS_CONNECTION_STATUS_FLAG_HAS_BEEN_BONDED_BEFORE 1
#define GG_LINK_STATUS_CONNECTION_STATUS_FLAG_ENCRYPTED              2
#define GG_LINK_STATUS_CONNECTION_STATUS_FLAG_DLE_ON                 4
#define GG_LINK_STATUS_CONNECTION_STATUS_FLAG_DLE_REBOOT_REQUIRED    8

#define GG_STACK_TOOL_DEFAULT_NODE_NAME "Jiji"

static const uint8_t GG_LinkConfigurationService_DefaultConnectionConfiguration[18] = {
    0x00,       // Field selection mask: all fields ignored
    0x00, 0x00, // Fast Mode Min Connection Interval
    0x00, 0x00, // Fast Mode Max Connection Interval
    0x00,       // Fast Mode Slave Latency
    0x00,       // Fast Mode Supervision Timeout
    0x00, 0x00, // Slow Mode Min Connection Interval
    0x00, 0x00, // Slow Mode Max Connection Interval
    0x00,       // Slow Mode Slave Latency
    0x00,       // Slow Mode Supervision Timeout
    0x00,       // DLE Max Tx PDU Size
    0x00, 0x00, // DLE Max Tx Time
    0x00, 0x00  // Prefered MTU Size
};
static uint8_t GG_LinkConfigurationService_ConnectionMode[1] = {
    0x00 // Fast mode
};

static const uint8_t GG_LinkStatusService_DefaultConnectionConfiguration[10] = {
    0x0C, 0x00, // Connection interval (15ms)
    0x00, 0x00, // Slave Latency
    0x90, 0x01, // Supervision Timeout (4000ms)
    0x17, 0x00, // ATT MTU (23)
    0x00        // Speed Mode (0 = default)
};
static const uint8_t GG_LinkStatusService_DefaultConnectionStatus[7] = {
    0x00,       // Flags
    0x00,       // Maximum Tx Payload Length
    0x00, 0x00, // Maximum Tx Time
    0x00,       // Maximum Rx Payload Length
    0x00, 0x00, // Maximum Rx Time
};

//----------------------------------------------------------------------
// logging
//----------------------------------------------------------------------
GG_SET_LOCAL_LOGGER("gg.xp.app.stack-tool.core-bluetooth")

//----------------------------------------------------------------------
// forward declarations
//----------------------------------------------------------------------
static void GG_StackToolBluetoothTransport_UpdateMtu(GG_StackToolBluetoothTransport* self, size_t mtu);
static void GG_StackToolBluetoothTransport_NotifyConnected(GG_StackToolBluetoothTransport* self);
static void GG_StackToolBluetoothTransport_OnDataReceived(GG_StackToolBluetoothTransport* self,
                                                          const void*                     data,
                                                          size_t                          data_size);
static void GG_StackToolBluetoothTransport_OnDeviceDiscovered(GG_StackToolBluetoothTransport* self,
                                                              const char*                     peripheral_name,
                                                              const char*                     peripheral_id,
                                                              int                             rssi);
static void GG_StackToolBluetoothTransport_OnLinkStatusConfigurationUpdated(GG_StackToolBluetoothTransport* self,
                                                                            unsigned int connection_interval,
                                                                            unsigned int slave_latency,
                                                                            unsigned int supervision_timeout,
                                                                            unsigned int mtu,
                                                                            unsigned int mode);

//----------------------------------------------------------------------
// HubBluetoothTransort class (inner Objective-C implemenation)
//----------------------------------------------------------------------
@interface HubBluetoothTransport: NSObject <CBCentralManagerDelegate,
                                            CBPeripheralManagerDelegate,
                                            CBPeripheralDelegate>

@property (strong, nonatomic) NSLock*                  lock;
@property (strong, nonatomic) NSUUID*                  target;
@property (strong, nonatomic) CBCentralManager*        centralManager;
@property (strong, nonatomic) CBPeripheralManager*     peripheralManager;
@property (strong, nonatomic) CBPeripheral*            peripheral;
@property (strong, nonatomic) CBMutableService*        linkConfigurationService;
@property (strong, nonatomic) CBMutableCharacteristic* linkConfigurationConnectionConfigurarionCharacteristic;
@property (strong, nonatomic) NSData*                  linkConfigurationConnectionConfigurarionCharacteristicValue;
@property (strong, nonatomic) CBCentral*               linkConfigurationConnectionConfigurationSubscriber;
@property (strong, nonatomic) CBMutableCharacteristic* linkConfigurationConnectionModeCharacteristic;
@property (strong, nonatomic) NSData*                  linkConfigurationConnectionModeCharacteristicValue;
@property (strong, nonatomic) CBCentral*               linkConfigurationConnectionModeSubscriber;
@property (strong, nonatomic) CBMutableCharacteristic* linkConfigurationGeneralPurposeCommandCharacteristic;
@property (strong, nonatomic) CBCentral*               linkConfigurationGeneralPurposeCommandSubscriber;
@property (strong, nonatomic) CBService*               gattConfirmationService;
@property (strong, nonatomic) CBUUID*                  gattConfirmationServiceEphemeralCharacteristicUuid;
@property (strong, nonatomic) CBCharacteristic*        gattlinkRxCharacteristic;
@property (nonatomic)         BOOL                     gattlinkRxOk;
@property (nonatomic)         BOOL                     gattlinkTxOk;
@property (strong, nonatomic) NSMutableArray*          rxQueue;
@property (nonatomic)         BOOL                     rxReady;
@property (nonatomic)         BOOL                     centralOn;
@property (nonatomic)         BOOL                     peripheralOn;

@property (nonatomic)         GG_StackToolBluetoothTransport* host; // reference to the owning object for callbacks

@end

@implementation HubBluetoothTransport

// Init a new instance
- (id) initWithHost: (GG_StackToolBluetoothTransport *)host target: (NSString *)target {
    self = [super init];
    if (self) {
        // create a mutex for this object
        self.lock = [[NSLock alloc] init];

        // create a queue for TX packets
        self.rxQueue = [[NSMutableArray alloc] init];
        self.rxReady = TRUE;

        // keep a reference to the host so we can call it back
        self.host = host;

        // convert the target to a CBUUID
        if (![target isEqualToString: @"scan"]) {
            self.target = [[NSUUID alloc] initWithUUIDString: target];
        }

        // create a GATT service for the Link Configuration Service
        self.linkConfigurationService = [[CBMutableService alloc]
            initWithType: [CBUUID UUIDWithString: GG_LINK_CONFIGURATION_SERVICE_UUID] primary: TRUE];
        self.linkConfigurationConnectionConfigurarionCharacteristic = [[CBMutableCharacteristic alloc]
            initWithType: [CBUUID UUIDWithString: GG_LINK_CONFIGURATION_CONNECTION_CONFIGURATION_CHARACTERISTIC_UUID]
              properties: CBCharacteristicPropertyNotify | CBCharacteristicPropertyRead
                   value: nil
             permissions: CBAttributePermissionsReadable];
        self.linkConfigurationConnectionConfigurarionCharacteristicValue =
            [NSData dataWithBytes: GG_LinkConfigurationService_DefaultConnectionConfiguration
                           length: sizeof(GG_LinkConfigurationService_DefaultConnectionConfiguration)];
        self.linkConfigurationConnectionModeCharacteristic = [[CBMutableCharacteristic alloc]
            initWithType: [CBUUID UUIDWithString: GG_LINK_CONFIGURATION_CONNECTION_MODE_CHARACTERISTIC_UUID]
              properties: CBCharacteristicPropertyNotify | CBCharacteristicPropertyRead
                   value: nil
             permissions: CBAttributePermissionsReadable];
        self.linkConfigurationConnectionModeCharacteristicValue =
            [NSData dataWithBytes: GG_LinkConfigurationService_ConnectionMode
                           length: sizeof(GG_LinkConfigurationService_ConnectionMode)];
        self.linkConfigurationGeneralPurposeCommandCharacteristic = [[CBMutableCharacteristic alloc]
            initWithType: [CBUUID UUIDWithString: GG_LINK_CONFIGURATION_GENERAL_PURPOSE_COMMAND_CHARACTERISTIC_UUID]
              properties: CBCharacteristicPropertyNotify
                   value: nil
             permissions: 0]; // no attribute read and write permisions; notify only
        self.linkConfigurationService.characteristics = @[
            self.linkConfigurationConnectionConfigurarionCharacteristic,
            self.linkConfigurationConnectionModeCharacteristic,
            self.linkConfigurationGeneralPurposeCommandCharacteristic
        ];
    }

    return self;
}

// Start the transport
- (void)start {
    // get a queue to do the central and peripheral work on
    dispatch_queue_t bt_queue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);

    // initialize the peripheral manager
    // NOTE: allocate first, then init, because init may invoke a delegate before returning,
    // which could end up referencing self.peripheralManager before it is assigned
    self.peripheralManager = [CBPeripheralManager alloc];
    [self.peripheralManager initWithDelegate: self queue: bt_queue];

    // initialize the central manager
    // NOTE: allocate first, then init, because init may invoke a delegate before returning,
    // which could end up referencing self.centralManager before it is assigned
    self.centralManager = [CBCentralManager alloc];
    [self.centralManager initWithDelegate: self queue: bt_queue];
}

// Look for a target
- (bool)findTarget {
    NSArray<CBPeripheral *>* connected =
        [self.centralManager retrieveConnectedPeripheralsWithServices: @[
            [CBUUID UUIDWithString: GG_GATTLINK_SERVICE_UUID],
            [CBUUID UUIDWithString: GG_LINK_STATUS_SERVICE_UUID]
        ]];
    for (CBPeripheral* peripheral in connected) {
        if (self.target && [peripheral.identifier.UUIDString isEqualTo: self.target.UUIDString]) {
            GG_LOG_INFO("found target, already connected");
            [self onTargetFound: peripheral];
            return true;
        }
    }

    return false;
}

// Connect to a peripheral when found
- (void)onTargetFound: (CBPeripheral*) peripheral {
    GG_LOG_INFO("target found");

    // Save a reference to the peripheral
    self.peripheral = peripheral;

    // Connect
    GG_LOG_FINE("connecting...");
    [self.centralManager connectPeripheral: peripheral options: nil];
}

// Start scanning
- (void)startScanning {
    if (!self.centralManager.isScanning) {
        // Scan for devices
        [self.centralManager scanForPeripheralsWithServices: @[
                [CBUUID UUIDWithString: GG_GATTLINK_SERVICE_UUID],
                [CBUUID UUIDWithString: GG_LINK_STATUS_SERVICE_UUID]
            ]
                                                    options: @{
                CBCentralManagerScanOptionAllowDuplicatesKey: @YES,
                CBCentralManagerScanOptionSolicitedServiceUUIDsKey: @[
                    [CBUUID UUIDWithString: GG_GATTLINK_SERVICE_UUID]
                ]
            }
        ];
        GG_LOG_INFO("scanning started");
    }
}

// Disconnect from any connected peripheral
- (void)disconnect {
    if (self.peripheral) {
        [self.centralManager cancelPeripheralConnection: self.peripheral];
    }
}

// Update the preferred connection mode
- (void)updatePreferredConnectionMode: (uint8_t) mode {
    if (self.peripheralManager == nil || self.linkConfigurationConnectionModeSubscriber == nil) {
        return;
    }
    GG_LinkConfigurationService_ConnectionMode[0] = mode;
    self.linkConfigurationConnectionModeCharacteristicValue =
        [NSData dataWithBytes: GG_LinkConfigurationService_ConnectionMode
                       length: sizeof(GG_LinkConfigurationService_ConnectionMode)];
    [self.peripheralManager updateValue: self.linkConfigurationConnectionModeCharacteristicValue
                      forCharacteristic: self.linkConfigurationConnectionModeCharacteristic
                   onSubscribedCentrals: @[self.linkConfigurationConnectionModeSubscriber]];
}

// Delegate method called when the central role is powered on or off
- (void)centralManagerDidUpdateState:(CBCentralManager *)central {
    GG_LOG_INFO("central manager state changed: %d", (int)central.state);
    if (central.state == CBManagerStatePoweredOn) {
        if (!self.centralOn) {
            self.centralOn = TRUE;
            if (![self findTarget]) {
                // target not found in connected devices, scan
                [self startScanning];
            }
        }
    } else if (central.state == CBManagerStatePoweredOff) {
        if (self.centralOn) {
            self.centralOn = FALSE;
            [self cleanupCentral];
        }
    }
}

// Delegate called when the peripheral role is powered on or off
- (void)peripheralManagerDidUpdateState: (nonnull CBPeripheralManager *)peripheral {
    GG_LOG_INFO("peripheral manager state changed: %d", (int)peripheral.state);
    if (peripheral.state == CBManagerStatePoweredOn) {
        if (!self.peripheralOn) {
            self.peripheralOn = TRUE;

            // Add the Link Configuration service
            [peripheral addService: self.linkConfigurationService];
        }
    } else if (peripheral.state == CBManagerStatePoweredOff) {
        if (self.peripheralOn) {
            self.peripheralOn = FALSE;
            [self cleanupPeripheral];
        }
    }
}

// Delegate method called when a service has been added
- (void)peripheralManager: (CBPeripheralManager *)peripheralManager
            didAddService: (CBService *)service
                    error: (NSError *)error {
    GG_COMPILER_UNUSED(peripheralManager);

    if (error) {
        GG_LOG_WARNING("failed to add services: %s", error.localizedDescription.UTF8String);
        return;
    }

    GG_LOG_FINE("service added: %s", service.UUID.UUIDString.UTF8String);
}

// Delegate method called when a peripheral has been discovered
- (void)centralManager: (CBCentralManager *)central
 didDiscoverPeripheral: (CBPeripheral *)peripheral
     advertisementData: (NSDictionary *)advertisementData
                  RSSI: (NSNumber *)RSSI {
    GG_COMPILER_UNUSED(central);
    GG_COMPILER_UNUSED(advertisementData);

    if (self.peripheral) {
        // we already have a peripheral, just ignore this
        GG_LOG_FINE("ignoring discovered peripheral, we already have one");
        return;
    }

    // if we're just scanning, notify the listener
    if (self.target == nil) {
        GG_StackToolBluetoothTransport_OnDeviceDiscovered(self.host,
                                                          peripheral.name.UTF8String,
                                                          peripheral.identifier.UUIDString.UTF8String,
                                                          RSSI.intValue);
    }

    // check if this is our target
    GG_LOG_INFO("found peripheral %s, rssi=%d, state=%d, id=%s",
                peripheral.name.UTF8String,
                RSSI.intValue,
                (int)peripheral.state,
                peripheral.identifier.UUIDString.UTF8String);
    if (self.target && [peripheral.identifier.UUIDString isEqualTo: self.target.UUIDString]) {
        [self onTargetFound: peripheral];
    }
}

// Delegate method called when a peripheral is connected
- (void)centralManager: (CBCentralManager *)central
  didConnectPeripheral: (CBPeripheral *)peripheral {
    GG_COMPILER_UNUSED(central);
    GG_COMPILER_UNUSED(peripheral);

    GG_LOG_INFO("connected!");

    // stop scanning
    [self.centralManager stopScan];

    // register as a delegate for the peripheral
    peripheral.delegate = self;

    // discover services
    [peripheral discoverServices: @[
        [CBUUID UUIDWithString: GG_GATTLINK_SERVICE_UUID],
        [CBUUID UUIDWithString: GG_LINK_STATUS_SERVICE_UUID],
        [CBUUID UUIDWithString: GG_GATT_CONFIRMATION_SERVICE_UUID]
    ]];
}

// Delegate method called when a peripheral is disconnected
-  (void)centralManager: (CBCentralManager *)central
didDisconnectPeripheral: (CBPeripheral *)peripheral
                  error: (NSError *)error {
    GG_COMPILER_UNUSED(central);
    GG_COMPILER_UNUSED(peripheral);
    GG_COMPILER_UNUSED(error);

    GG_LOG_INFO("disconnected!");

    // cleanup
    self.linkConfigurationConnectionModeSubscriber = nil;
    self.linkConfigurationConnectionConfigurationSubscriber = nil;
    self.gattlinkRxCharacteristic = nil;
    self.gattConfirmationService = nil;
    self.gattConfirmationServiceEphemeralCharacteristicUuid = nil;
    self.gattlinkTxOk = false;
    self.gattlinkRxOk = false;
    self.peripheral= nil;

    [self startScanning];
}

// Delegate method called when a connection failed
-     (void)centralManager: (CBCentralManager *)central
didFailToConnectPeripheral: (CBPeripheral *)peripheral
                     error: (NSError *)error {
    GG_COMPILER_UNUSED(central);
    GG_COMPILER_UNUSED(peripheral);

    GG_LOG_WARNING("failed to connect: %s", error ? error.localizedDescription.UTF8String : "");
    [self cleanupCentral];
}

// Delegate method called when services have been discovered
- (void)peripheral: (CBPeripheral *)peripheral didDiscoverServices: (NSError *)error {
    if (error) {
        GG_LOG_WARNING("service discovery failed");
        [self cleanupCentral];
        return;
    }

    // check which services have been discovered, and discover their characteristics
    for (CBService* service in peripheral.services) {
        GG_LOG_FINER("service discovered: %s", service.UUID.UUIDString.UTF8String);
        if ([service.UUID isEqual: [CBUUID UUIDWithString: GG_GATTLINK_SERVICE_UUID]]) {
            // Gattlink Service
            // discover the TX and RX characteristics
            GG_LOG_FINE("Gattlink Service discovered");
            [peripheral discoverCharacteristics: @[
                    [CBUUID UUIDWithString: GG_GATTLINK_TX_CHARACTERISTIC_UUID],
                    [CBUUID UUIDWithString: GG_GATTLINK_RX_CHARACTERISTIC_UUID]
                ] forService: service];
        } else if ([service.UUID isEqual: [CBUUID UUIDWithString: GG_LINK_STATUS_SERVICE_UUID]]) {
            // Link Status Service
            // discover the connection configuration and connection status characteristics
            GG_LOG_FINE("Link Status Service discovered");
            [peripheral discoverCharacteristics: @[
                    [CBUUID UUIDWithString: GG_LINK_STATUS_CONNECTION_CONFIGURATION_CHARACTERISTIC_UUID],
                    [CBUUID UUIDWithString: GG_LINK_STATUS_CONNECTION_STATUS_CHARACTERISTIC_UUID]
                ] forService: service];
        } else if ([service.UUID isEqual: [CBUUID UUIDWithString: GG_GATT_CONFIRMATION_SERVICE_UUID]]) {
            // GATT Confirmation Service
            // discover the ephemeral characteristic pointer characteristics
            GG_LOG_FINE("GATT Confirmation Service discovered");
            self.gattConfirmationService = service;
            [peripheral discoverCharacteristics: @[
                [CBUUID UUIDWithString: GG_GATT_CONFIRMATION_EPHEMERAL_CHARACTERISTIC_POINTER_UUID]
                ] forService: service];
        }
    }
}

// Delegate method called when a service has changed
- (void)peripheral:(CBPeripheral *)peripheral
 didModifyServices:(NSArray<CBService *> *)invalidatedServices {
    NSMutableArray* services_to_rediscover = [[NSMutableArray alloc] init];
    for (CBService* service in invalidatedServices) {
        GG_LOG_FINE("service change indication for %s", service.UUID.UUIDString.UTF8String);
        if ([service.UUID isEqual: [CBUUID UUIDWithString: GG_GATTLINK_SERVICE_UUID]]) {
            GG_LOG_INFO("Gattlink Service changed");
            self.gattlinkRxOk = false;
            self.gattlinkTxOk = false;
            self.gattlinkRxCharacteristic = nil;
            [services_to_rediscover addObject: service.UUID];
        } else if ([service.UUID isEqual: [CBUUID UUIDWithString: GG_LINK_STATUS_SERVICE_UUID]]) {
            GG_LOG_INFO("Link Status Service changed");
            [services_to_rediscover addObject: service.UUID];
        }
    }

    // re-discover changed services
    if (services_to_rediscover.count) {
        [peripheral discoverServices: services_to_rediscover];
    }
}

// Delegate method called when characteristics have been discovered for a service
-                   (void)peripheral: (CBPeripheral *)peripheral
didDiscoverCharacteristicsForService: (CBService *)service
                               error: (NSError *)error {
    if (error) {
        GG_LOG_WARNING("characteristic discovery failed");
        [self cleanupCentral];
        return;
    }

    // subscribe to the Connection Config and Connection Status characteristics of the Link Status service
    // and the Gattlink TX characteristic
    GG_LOG_FINER("discovered characteristics for service %s", service.UUID.UUIDString.UTF8String);
    for (CBCharacteristic* characteristic in service.characteristics) {
        if ([characteristic.UUID isEqual:
                [CBUUID UUIDWithString: GG_LINK_STATUS_CONNECTION_CONFIGURATION_CHARACTERISTIC_UUID]]) {
            GG_LOG_FINE("subscribing to the Link Status Connection Configuration Characteristic");
            [peripheral setNotifyValue: YES forCharacteristic: characteristic];
        } else if ([characteristic.UUID isEqual:
                       [CBUUID UUIDWithString: GG_LINK_STATUS_CONNECTION_STATUS_CHARACTERISTIC_UUID]]) {
            GG_LOG_FINE("subscribing to the Link Status Connection Status Characteristic");
            [peripheral setNotifyValue: YES forCharacteristic: characteristic];
        } else if ([characteristic.UUID isEqual: [CBUUID UUIDWithString: GG_GATTLINK_RX_CHARACTERISTIC_UUID]]) {
            GG_LOG_FINE("found Gattlink RX");
            if (!self.gattlinkRxOk) {
                self.gattlinkRxOk = true;
                if (self.gattlinkTxOk) {
                    GG_StackToolBluetoothTransport_NotifyConnected(self.host);
                }
            }
            self.gattlinkRxCharacteristic = characteristic;

            size_t mtu = [peripheral maximumWriteValueLengthForType: CBCharacteristicWriteWithoutResponse];
            GG_LOG_FINE("connection MTU: %d", (int)mtu);
            if (mtu > GG_STACK_TOOL_MAX_MTU) {
                GG_LOG_FINE("clamping MTU to %d", (int)GG_STACK_TOOL_MAX_MTU);
                mtu = GG_STACK_TOOL_MAX_MTU;
            }
            GG_StackToolBluetoothTransport_UpdateMtu(self.host, mtu);
        } else if ([characteristic.UUID isEqual: [CBUUID UUIDWithString: GG_GATTLINK_TX_CHARACTERISTIC_UUID]]) {
            GG_LOG_FINE("found Gattlink TX");
            [peripheral setNotifyValue: YES forCharacteristic: characteristic];
        } else if ([characteristic.UUID isEqual:
            [CBUUID UUIDWithString: GG_GATT_CONFIRMATION_EPHEMERAL_CHARACTERISTIC_POINTER_UUID]]) {
            GG_LOG_FINE("found ephermeral characteristic pointer characteristic");
            if (!self.gattConfirmationServiceEphemeralCharacteristicUuid) {
                GG_LOG_FINER("reading value of ephermeral characteristic pointer");
                [peripheral readValueForCharacteristic: characteristic];
            }
        } else if (self.gattConfirmationServiceEphemeralCharacteristicUuid &&
                   [characteristic.UUID isEqual: self.gattConfirmationServiceEphemeralCharacteristicUuid]) {
            GG_LOG_FINE("found ephermeral characteristic, writing 0");
            uint8_t zero = 0;
            [peripheral writeValue: [NSData dataWithBytes: &zero length: 1]
                 forCharacteristic: characteristic
                              type: CBCharacteristicWriteWithResponse];
        }
    }
}

// Delegate method called when we have subscribed to a charcteristic
-                          (void)peripheral:(CBPeripheral *)peripheral
didUpdateNotificationStateForCharacteristic:(CBCharacteristic *)characteristic
                                      error:(NSError *)error {
    GG_COMPILER_UNUSED(peripheral);

    if (error) {
        GG_LOG_WARNING("subscription to %s failed", characteristic.UUID.UUIDString.UTF8String);
        return;
    }

    if ([characteristic.UUID isEqual: [CBUUID UUIDWithString: GG_GATTLINK_TX_CHARACTERISTIC_UUID]]) {
        GG_LOG_FINE("subscribed to Gattlink TX");
        if (!self.gattlinkTxOk) {
            self.gattlinkTxOk = true;
            if (self.gattlinkRxOk) {
                GG_StackToolBluetoothTransport_NotifyConnected(self.host);
            }
        }
    } else if ([characteristic.UUID isEqual:
               [CBUUID UUIDWithString: GG_LINK_STATUS_CONNECTION_CONFIGURATION_CHARACTERISTIC_UUID]]) {
        GG_LOG_FINE("subscribed to the Link Status Connection Configuration charateristic");
        [peripheral readValueForCharacteristic: characteristic];
    } else if ([characteristic.UUID isEqual:
               [CBUUID UUIDWithString: GG_LINK_STATUS_CONNECTION_STATUS_CHARACTERISTIC_UUID]]) {
        GG_LOG_FINE("subscribed to the Link Status Connection Status characteristic");
        [peripheral readValueForCharacteristic: characteristic];
    }
}

// Delegate method called when a GATT client subscribes to one of our characteristics
-    (void)peripheralManager: (CBPeripheralManager *)peripheral
                     central: (CBCentral *)central
didSubscribeToCharacteristic: (CBCharacteristic *)characteristic {
    GG_COMPILER_UNUSED(peripheral);

    GG_LOG_FINER("characteristic subscription: %s", characteristic.UUID.UUIDString.UTF8String);

    if ([characteristic.UUID isEqual: [CBUUID UUIDWithString: GG_GATTLINK_TX_CHARACTERISTIC_UUID]]) {
        GG_LOG_FINE("subscription to Gattlink TX");
    } else if ([characteristic.UUID isEqual:
        [CBUUID UUIDWithString: GG_LINK_CONFIGURATION_CONNECTION_CONFIGURATION_CHARACTERISTIC_UUID]]) {
        GG_LOG_FINE("subscription to the Link Configuration Connection Configuration Characteristic");

        // keep a reference to the subscriber
        self.linkConfigurationConnectionConfigurationSubscriber = central;
    } else if ([characteristic.UUID isEqual:
        [CBUUID UUIDWithString: GG_LINK_CONFIGURATION_CONNECTION_MODE_CHARACTERISTIC_UUID]]) {
        GG_LOG_FINE("subscription to the Link Configuration Connection Mode Characteristic");

        // keep a reference to the subscriber
        self.linkConfigurationConnectionModeSubscriber = central;
    }
}

// Delegate method called when a GATT client unsubscribes from one of our characteristics
-        (void)peripheralManager: (CBPeripheralManager *)peripheral
                         central: (CBCentral *)central
didUnsubscribeFromCharacteristic: (CBCharacteristic *)characteristic {
    GG_COMPILER_UNUSED(peripheral);
    GG_COMPILER_UNUSED(central);

    GG_LOG_FINER("characteristic un-subscription: %s", characteristic.UUID.UUIDString.UTF8String);

    if ([characteristic.UUID isEqual:
        [CBUUID UUIDWithString: GG_LINK_CONFIGURATION_CONNECTION_CONFIGURATION_CHARACTERISTIC_UUID]]) {

        GG_LOG_FINE("un-subscription from the Link Configuration Connection Configuration Characteristic");

        // clear the subscriber if we have one
        self.linkConfigurationConnectionConfigurationSubscriber = nil;
    } else if ([characteristic.UUID isEqual:
        [CBUUID UUIDWithString: GG_LINK_CONFIGURATION_CONNECTION_MODE_CHARACTERISTIC_UUID]]) {

        GG_LOG_FINE("un-subscription from the Link Configuration Connection Mode Characteristic");

        // clear the subscriber if we have one
        self.linkConfigurationConnectionModeSubscriber = nil;
    }
}

// Delegate method called when we receive a notification of a characteristic value change or have read
// a characteristic value
-              (void)peripheral: (CBPeripheral *)peripheral
didUpdateValueForCharacteristic: (CBCharacteristic *)characteristic
                          error: (NSError *)error {
    GG_COMPILER_UNUSED(peripheral);
    GG_COMPILER_UNUSED(error);

    if (error) {
        GG_LOG_WARNING("characteristic value update error: %s", error.localizedDescription.UTF8String);
        return;
    }

    if ([characteristic.UUID isEqual: [CBUUID UUIDWithString: GG_GATTLINK_TX_CHARACTERISTIC_UUID]]) {
        GG_LOG_FINER("<<< gattlink TX, size=%u", (int)characteristic.value.length);
        GG_StackToolBluetoothTransport_OnDataReceived(self.host,
                                                      characteristic.value.bytes,
                                                      (size_t)characteristic.value.length);
    } else if ([characteristic.UUID isEqual:
        [CBUUID UUIDWithString: GG_GATT_CONFIRMATION_EPHEMERAL_CHARACTERISTIC_POINTER_UUID]]) {
        GG_LOG_FINE("read ephermeral characteristic pointer value");
        if (characteristic.value == nil || characteristic.value.length != 16) {
            GG_LOG_WARNING("value is not 16 bytes");
            return;
        }
        NSData* value = characteristic.value; // copy to avoid non-nullable warning
        self.gattConfirmationServiceEphemeralCharacteristicUuid = [CBUUID UUIDWithData: value];
        GG_LOG_INFO("ephemeral characteristic UUID = %s",
                    self.gattConfirmationServiceEphemeralCharacteristicUuid.UUIDString.UTF8String);
        if (self.gattConfirmationService) {
            [peripheral discoverCharacteristics: @[ self.gattConfirmationServiceEphemeralCharacteristicUuid ]
                                     forService: self.gattConfirmationService];
        } else {
            GG_LOG_WARNING("gattConfirmationServiceEphemeralCharacteristicUuid is NIL");
        }
    } else if ([characteristic.UUID isEqual:
                   [CBUUID UUIDWithString: GG_LINK_STATUS_CONNECTION_CONFIGURATION_CHARACTERISTIC_UUID]]) {
        GG_LOG_FINE("got value for the Link Status Connection Configuration Charateristic");
        if (characteristic.value == nil | characteristic.value.length < 9) {
            GG_LOG_WARNING("characteristic value too short");
            return;
        }
        const uint8_t* data = characteristic.value.bytes;
        uint16_t connection_interval = GG_BytesToInt16Le(data);
        uint16_t slave_latency       = GG_BytesToInt16Le(data + 2);
        uint16_t supervision_timeout = GG_BytesToInt16Le(data + 4);
        uint16_t mtu                 = GG_BytesToInt16Le(data + 6);
        uint8_t  mode                = data[8];
        GG_LOG_INFO("    connection_interval: %d.%02d ms", (connection_interval * 5) / 4,
                                                           (connection_interval * 500 / 4) % 100);
        GG_LOG_INFO("    slave_latency:       %d", slave_latency);
        GG_LOG_INFO("    supervision_timeout: %d ms", supervision_timeout * 10);
        GG_LOG_INFO("    mtu:                 %d", mtu);
        switch (mode) {
            case 0:
                GG_LOG_INFO("    mode:                default");
                break;

            case 1:
                GG_LOG_INFO("    mode:                fast");
                break;

            case 2:
                GG_LOG_INFO("    mode:                slow");
                break;

            default:
                GG_LOG_INFO("    mode:                %d", mode);
                break;
        }

        GG_StackToolBluetoothTransport_OnLinkStatusConfigurationUpdated(self.host,
                                                                        connection_interval,
                                                                        slave_latency,
                                                                        supervision_timeout * 10,
                                                                        mtu,
                                                                        mode);
    } else if ([characteristic.UUID isEqual:
                   [CBUUID UUIDWithString: GG_LINK_STATUS_CONNECTION_STATUS_CHARACTERISTIC_UUID]]) {
        GG_LOG_FINE("got value for the Link Status Connection Status characteristic");
        if (characteristic.value == nil | characteristic.value.length < 7) {
            GG_LOG_WARNING("characteristic value too short");
            return;
        }
        const uint8_t* data = characteristic.value.bytes;
        uint8_t flags = data[0];
        uint8_t dle_max_tx_pdu_size = data[1];
        uint16_t dle_max_tx_time = GG_BytesToInt16Le(data + 2);
        uint8_t dle_max_rx_pdu_size = data[4];
        uint16_t dle_max_rx_time = GG_BytesToInt16Le(data + 5);
        GG_LOG_INFO("    bonded:              %s", (flags & GG_LINK_STATUS_CONNECTION_STATUS_FLAG_HAS_BEEN_BONDED_BEFORE) ? "yes" : "no");
        GG_LOG_INFO("    encrypted:           %s", (flags & GG_LINK_STATUS_CONNECTION_STATUS_FLAG_ENCRYPTED) ? "yes" : "no");
        GG_LOG_INFO("    DLE on:              %s", (flags & GG_LINK_STATUS_CONNECTION_STATUS_FLAG_DLE_ON) ? "yes" : "no");
        GG_LOG_INFO("    DLE requires reboot: %s", (flags & GG_LINK_STATUS_CONNECTION_STATUS_FLAG_DLE_REBOOT_REQUIRED) ? "yes" : "no");
        GG_LOG_INFO("    dle_max_tx_pdu_size: %d", dle_max_tx_pdu_size);
        GG_LOG_INFO("    dle_max_tx_time:     %d", dle_max_tx_time);
        GG_LOG_INFO("    dle_max_rx_pdu_size: %d", dle_max_rx_pdu_size);
        GG_LOG_INFO("    dle_max_rx_time:     %d", dle_max_rx_time);
    }
}

// Delegate method called when a GATT client writes to one of our characteristics
- (void)peripheralManager: (CBPeripheralManager *)peripheral
  didReceiveWriteRequests: (NSArray<CBATTRequest *> *)requests {
    GG_COMPILER_UNUSED(peripheral);

    // sanity check
    if (requests.count != 1) {
        GG_LOG_WARNING("unexpected multiple write request (count = %u)", (int)requests.count);
        return;
    }
}

// Delegate method called when a GATT client reads one of our characteristics
- (void)peripheralManager: (CBPeripheralManager *)peripheral
    didReceiveReadRequest: (CBATTRequest *)request {
    GG_LOG_FINER("received read request for %s", request.characteristic.UUID.UUIDString.UTF8String);

    if ([request.characteristic.UUID isEqual:
        [CBUUID UUIDWithString: GG_LINK_CONFIGURATION_CONNECTION_CONFIGURATION_CHARACTERISTIC_UUID]]) {
        request.value = self.linkConfigurationConnectionConfigurarionCharacteristicValue;
        [peripheral respondToRequest: request withResult: CBATTErrorSuccess];
    } else if ([request.characteristic.UUID isEqual:
        [CBUUID UUIDWithString: GG_LINK_CONFIGURATION_CONNECTION_MODE_CHARACTERISTIC_UUID]]) {
        request.value = self.linkConfigurationConnectionModeCharacteristicValue;
        [peripheral respondToRequest: request withResult: CBATTErrorSuccess];
    } else if ([request.characteristic.UUID isEqual:
        [CBUUID UUIDWithString: GG_GATTLINK_TX_CHARACTERISTIC_UUID]]) {
        // we shouldn't get read requests for this, as this should be a notif-only characteristic
        // but it seems that not responding to reads confuses some clients
        request.value = nil;
        [peripheral respondToRequest: request withResult: CBATTErrorSuccess];
    }
}

// Delegate method called when it is Ok to write
- (void)peripheralIsReadyToSendWriteWithoutResponse:(CBPeripheral *)peripheral {
    GG_COMPILER_UNUSED(peripheral);

    [self.lock lock];

    // try to send
    self.rxReady = TRUE;
    [self checkRxQueue];

    [self.lock unlock];
}

// Try to send data from the RX queue
// NOTE: the lock must be held
- (void)checkRxQueue {
    // do nothing if we're waiting for RX to be ready
    if (!self.rxReady || !self.peripheral || !self.gattlinkRxCharacteristic) {
        return;
    }

    // try to send more if we can
    if (self.rxQueue.count) {
        // check if we can write
#if 0
        // NOTE the following block has been temporarily disabled, because canSendWriteWithoutResponse
        // is broken on some version of macOS, where it always returns FALSE. Until we can be sure that
        // most users have moved to Mojave or later, where this works, we'll leave this check disabled,
        // which isn't an issue on a mac platform, because the write queue is large enough to accept
        // entire Gattlink windows without any problem.
        BOOL canWrite = [self.peripheral canSendWriteWithoutResponse];
        if (!canWrite) {
            GG_LOG_WARNING("can't write at this time, will try again later");
            self.rxReady = FALSE;
            return;
        }
#endif

        // write
        NSData* data = self.rxQueue[self.rxQueue.count - 1];
        [self.peripheral writeValue: data
                  forCharacteristic: self.gattlinkRxCharacteristic
                               type: CBCharacteristicWriteWithoutResponse];

        // data sent, remove from the queue
        GG_LOG_FINER(">>> gattlink RX, size=%u", (int)data.length);
        [self.rxQueue removeLastObject];
    }
}

// Send data to Gattlink
- (void)sendData: (NSData *)data {
    (void)data;
    if (!self.gattlinkRxCharacteristic) {
        GG_LOG_WARNING("no characteristic, dropping gattlink RX data");
        return;
    }

    // lock before we check the queue
    [self.lock lock];

    // check if the queue is full
    if (self.rxQueue.count >= GG_STACK_TOOL_RX_QUEUE_SIZE) {
        GG_LOG_WARNING("RX queue full, data dropped");
    } else {
        // add to the front of the queue
        [self.rxQueue insertObject: data atIndex: 0];

        // send anything that's ready
        [self checkRxQueue];
    }

    [self.lock unlock];
}

// Release central resources that were obtained
- (void)cleanupCentral {
    // stop scanning
    if (self.centralManager.isScanning) {
        [self.centralManager stopScan];
    }

    // unsubscribe from characteristics
    if (self.peripheral) {
        if (self.peripheral.services) {
            for (CBService* service in self.peripheral.services) {
                if (service.characteristics) {
                    for (CBCharacteristic* characteristic in service.characteristics) {
                        if (characteristic.isNotifying) {
                            [self.peripheral setNotifyValue: NO forCharacteristic: characteristic];
                            return;
                        }
                    }
                }
            }
        }
    }

    // cancel the connection
    if (self.centralManager && self.peripheral) {
        [self.centralManager cancelPeripheralConnection: self.peripheral];
    }

    // release references
    self.linkConfigurationConnectionConfigurationSubscriber = nil;
    self.linkConfigurationConnectionModeSubscriber = nil;
    self.gattlinkRxCharacteristic = nil;
    self.gattConfirmationService = nil;
    self.gattConfirmationServiceEphemeralCharacteristicUuid = nil;
    self.gattlinkTxOk = false;
    self.gattlinkRxOk = false;
    self.peripheral= nil;
}

// Release peripheral resources that were obtained
- (void)cleanupPeripheral {
    // unpublish services
    if (self.peripheralManager) {
        [self.peripheralManager removeAllServices];
    }
}

@end

//----------------------------------------------------------------------
// NodeBluetoothTransort class (inner Objective-C implemenation)
//----------------------------------------------------------------------
@interface NodeBluetoothTransport: NSObject <CBCentralManagerDelegate,
                                             CBPeripheralManagerDelegate,
                                             CBPeripheralDelegate>

@property (strong, nonatomic) NSString*                advertisedName;
@property (strong, nonatomic) NSLock*                  lock;
@property (strong, nonatomic) CBCentralManager*        centralManager;
@property (strong, nonatomic) CBPeripheralManager*     peripheralManager;
@property (strong, nonatomic) CBPeripheral*            peer;
@property (strong, nonatomic) CBMutableService*        gattlinkService;
@property (strong, nonatomic) CBMutableCharacteristic* gattlinkRxCharacteristic;
@property (strong, nonatomic) CBMutableCharacteristic* gattlinkTxCharacteristic;
@property (strong, nonatomic) CBCentral*               gattlinkSubscriber;
@property (strong, nonatomic) CBMutableService*        linkStatusService;
@property (strong, nonatomic) CBMutableCharacteristic* linkStatusConnectionConfigurationCharacteristic;
@property (strong, nonatomic) NSMutableData*           linkStatusConnectionConfigurationCharacteristicValue;
@property (nonatomic)         BOOL                     linkStatusConnectionConfigurationCharacteristicChanged;
@property (strong, nonatomic) CBCentral*               linkStatusConnectionConfigurationSubscriber;
@property (strong, nonatomic) CBMutableCharacteristic* linkStatusConnectionStatusCharacteristic;
@property (strong, nonatomic) NSMutableData*           linkStatusConnectionStatusCharacteristicValue;
@property (nonatomic)         BOOL                     linkStatusConnectionStatusCharacteristicChanged;
@property (strong, nonatomic) CBCentral*               linkStatusConnectionStatusSubscriber;
@property (strong, nonatomic) CBMutableCharacteristic* linkStatusSecureCharacteristic;
@property (strong, nonatomic) NSMutableArray*          txQueue;
@property (nonatomic)         BOOL                     txReady;
@property (nonatomic)         BOOL                     centralOn;
@property (nonatomic)         BOOL                     peripheralOn;

@property (nonatomic)         GG_StackToolBluetoothTransport* host; // reference to the owning object for callbacks

@end

@implementation NodeBluetoothTransport

// Init a new instance
- (id) initWithHost: (GG_StackToolBluetoothTransport *)host
     advertisedName: (NSString*)advertisedName {
    self = [super init];
    if (self) {
        // set our name
        self.advertisedName = advertisedName;

        // create a mutex for this object
        self.lock = [[NSLock alloc] init];

        // create a queue for TX packets
        self.txQueue = [[NSMutableArray alloc] init];
        self.txReady = TRUE;

        // keep a reference to the host so we can call it back
        self.host = host;

        // create a GATT service for the Gattlink Service
        self.gattlinkService = [[CBMutableService alloc] initWithType: [CBUUID UUIDWithString: GG_GATTLINK_SERVICE_UUID]
                                                              primary: TRUE];
        self.gattlinkRxCharacteristic = [[CBMutableCharacteristic alloc]
            initWithType: [CBUUID UUIDWithString: GG_GATTLINK_RX_CHARACTERISTIC_UUID]
              properties: CBCharacteristicPropertyWriteWithoutResponse
                   value: nil
             permissions: CBAttributePermissionsWriteable];
        self.gattlinkTxCharacteristic = [[CBMutableCharacteristic alloc]
            initWithType: [CBUUID UUIDWithString: GG_GATTLINK_TX_CHARACTERISTIC_UUID]
              properties: CBCharacteristicPropertyNotify | CBCharacteristicPropertyRead
                   value: nil
             permissions: CBAttributePermissionsReadable];
        self.gattlinkService.characteristics = @[self.gattlinkRxCharacteristic, self.gattlinkTxCharacteristic];

        // create a GATT service for the Link Status Service
        self.linkStatusService = [[CBMutableService alloc]
            initWithType: [CBUUID UUIDWithString: GG_LINK_STATUS_SERVICE_UUID] primary: TRUE];
        self.linkStatusConnectionConfigurationCharacteristic = [[CBMutableCharacteristic alloc]
            initWithType: [CBUUID UUIDWithString: GG_LINK_STATUS_CONNECTION_CONFIGURATION_CHARACTERISTIC_UUID]
              properties: CBCharacteristicPropertyNotify | CBCharacteristicPropertyRead
                   value: nil
             permissions: CBAttributePermissionsReadable];
        self.linkStatusConnectionConfigurationCharacteristicValue =
            [NSMutableData dataWithBytes: GG_LinkStatusService_DefaultConnectionConfiguration
                                  length: sizeof(GG_LinkStatusService_DefaultConnectionConfiguration)];
        self.linkStatusConnectionStatusCharacteristic = [[CBMutableCharacteristic alloc]
            initWithType: [CBUUID UUIDWithString: GG_LINK_STATUS_CONNECTION_STATUS_CHARACTERISTIC_UUID]
              properties: CBCharacteristicPropertyNotify | CBCharacteristicPropertyRead
                   value: nil
             permissions: CBAttributePermissionsReadable];
        self.linkStatusConnectionStatusCharacteristicValue =
            [NSMutableData dataWithBytes: GG_LinkStatusService_DefaultConnectionStatus
                                  length: sizeof(GG_LinkStatusService_DefaultConnectionStatus)];
        uint8_t zero = 0;
        self.linkStatusSecureCharacteristic = [[CBMutableCharacteristic alloc]
            initWithType: [CBUUID UUIDWithString: GG_LINK_STATUS_SECURE_CHARACTERISTIC_UUID]
              properties: CBCharacteristicPropertyRead
                   value: [NSData dataWithBytes: &zero length: 1]
             permissions: CBAttributePermissionsReadEncryptionRequired];
        self.linkStatusService.characteristics = @[
            self.linkStatusConnectionConfigurationCharacteristic,
            self.linkStatusConnectionStatusCharacteristic,
            self.linkStatusSecureCharacteristic
        ];
    }

    return self;
}

// Start the transport
- (void)start {
    // get a queue to do the central and peripheral work on
    dispatch_queue_t bt_queue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);

    // initialize the peripheral manager
    // NOTE: allocate first, then init, because init may invoke a delegate before returning,
    // which could end up referencing self.peripheralManager before it is assigned
    self.peripheralManager = [CBPeripheralManager alloc];
    [self.peripheralManager initWithDelegate: self queue: bt_queue];

    // initialize the central manager
    // NOTE: allocate first, then init, because init may invoke a delegate before returning,
    // which could end up referencing self.centralManager before it is assigned
    self.centralManager = [CBCentralManager alloc];
    [self.centralManager initWithDelegate: self queue: bt_queue];
}

// Delegate method called when the central role is powered on or off
- (void)centralManagerDidUpdateState:(CBCentralManager *)central {
    GG_LOG_INFO("central manager state changed: %d", (int)central.state);
    if (central.state == CBManagerStatePoweredOn) {
        if (!self.centralOn) {
            self.centralOn = TRUE;
        }
    } else if (central.state == CBManagerStatePoweredOff) {
        if (self.centralOn) {
            self.centralOn = FALSE;
            [self cleanupCentral];
        }
    }
}

// Delegate called when the peripheral role is powered on or off
- (void)peripheralManagerDidUpdateState: (nonnull CBPeripheralManager *)peripheral {
    GG_LOG_INFO("peripheral manager state changed: %d", (int)peripheral.state);
    if (peripheral.state == CBManagerStatePoweredOn) {
        if (!self.peripheralOn) {
            self.peripheralOn = TRUE;

            // Add the Gattlink service
            [peripheral addService: self.gattlinkService];

            // Add the Link Status service
            [peripheral addService: self.linkStatusService];

            // advertise our name and the Golden Gate Service
            [self.peripheralManager startAdvertising: @{
                CBAdvertisementDataLocalNameKey: self.advertisedName,
                CBAdvertisementDataServiceUUIDsKey: @[[CBUUID UUIDWithString: GG_GATTLINK_SERVICE_UUID]]
            }];
        }
    } else if (peripheral.state == CBManagerStatePoweredOff) {
        if (self.peripheralOn) {
            self.peripheralOn = FALSE;
            [self.peripheralManager stopAdvertising];
            [self cleanupPeripheral];
        }
    }
}

// Delegate method called when we start advertisting
- (void)peripheralManagerDidStartAdvertising:(CBPeripheralManager *)peripheral
                                       error:(NSError *)error {
    GG_COMPILER_UNUSED(peripheral);

    if (error) {
        GG_LOG_WARNING("failed to advertise: %s", error.localizedDescription.UTF8String);
        return;
    }

    GG_LOG_INFO("advertising as %s", self.advertisedName.UTF8String);
}

// Delegate method called when a service has been added
- (void)peripheralManager: (CBPeripheralManager *)peripheralManager
            didAddService: (CBService *)service
                    error: (NSError *)error {
    GG_COMPILER_UNUSED(peripheralManager);

    if (error) {
        GG_LOG_WARNING("failed to add services: %s", error.localizedDescription.UTF8String);
        return;
    }

    GG_LOG_FINE("service added: %s", service.UUID.UUIDString.UTF8String);
}

// Delegate method called when services have been discovered
- (void)peripheral: (CBPeripheral *)peripheral didDiscoverServices: (NSError *)error {
    if (error) {
        GG_LOG_WARNING("service discovery failed: %s", error.localizedDescription.UTF8String);
        [self cleanupCentral];
        return;
    }

    // check which services have been discovered, and discover their characteristics
    for (CBService* service in peripheral.services) {
        GG_LOG_FINER("service discovered: %s", service.UUID.UUIDString.UTF8String);
        if ([service.UUID isEqual: [CBUUID UUIDWithString: GG_GATTLINK_SERVICE_UUID]]) {
            // Gattlink service
            // discover the RX and TX characteristics
            GG_LOG_FINE("discovered Gattlink Service");
            [peripheral discoverCharacteristics: @[
                    [CBUUID UUIDWithString: GG_GATTLINK_RX_CHARACTERISTIC_UUID],
                    [CBUUID UUIDWithString: GG_GATTLINK_TX_CHARACTERISTIC_UUID]
                ] forService: service];
        }
    }
}

// Delegate method called when characteristics have been discovered for a service
-                   (void)peripheral: (CBPeripheral *)peripheral
didDiscoverCharacteristicsForService: (CBService *)service
                               error: (NSError *)error {
    GG_COMPILER_UNUSED(peripheral);

    if (error) {
        GG_LOG_WARNING("characteristic discovery failed: %s", error.localizedDescription.UTF8String);
        [self cleanupCentral];
        return;
    }

    GG_LOG_FINER("discovered characteristics for service %s", service.UUID.UUIDString.UTF8String);
}

// Delegate method called when a service has changed
- (void)peripheral:(CBPeripheral *)peripheral
 didModifyServices:(NSArray<CBService *> *)invalidatedServices {
    NSMutableArray* services_to_rediscover = [[NSMutableArray alloc] init];
    for (CBService* service in invalidatedServices) {
        GG_LOG_INFO("service change indication for %s", service.UUID.UUIDString.UTF8String);
        if ([service.UUID isEqual: [CBUUID UUIDWithString: GG_LINK_CONFIGURATION_SERVICE_UUID]]) {
            GG_LOG_INFO("Link Configuration Service changed");
            [services_to_rediscover addObject: service.UUID];
        }
    }

    // re-discover changed services
    if (services_to_rediscover.count) {
        [peripheral discoverServices: services_to_rediscover];
    }
}

// Delegate method called when a GATT client subscribes to one of our characteristics
-    (void)peripheralManager: (CBPeripheralManager *)peripheral
                     central: (CBCentral *)central
didSubscribeToCharacteristic: (CBCharacteristic *)characteristic {
    GG_COMPILER_UNUSED(peripheral);
    GG_LOG_FINER("characteristic subscription: %s", characteristic.UUID.UUIDString.UTF8String);

    if ([characteristic.UUID isEqual:
        [CBUUID UUIDWithString: GG_LINK_STATUS_CONNECTION_CONFIGURATION_CHARACTERISTIC_UUID]]) {
        GG_LOG_FINE("subscription to the Link Status Connection Configuration characteristic");
        self.linkStatusConnectionConfigurationSubscriber = central;

        // update the Link Connection Configuration characteristic
        [self.lock lock];

        size_t mtu = central.maximumUpdateValueLength;
        uint8_t* value = (uint8_t*)[self.linkStatusConnectionConfigurationCharacteristicValue mutableBytes];
        value[6] = (uint8_t)((mtu + 3))      & 0xFF;
        value[7] = (uint8_t)((mtu + 3) >> 8) & 0xFF;
        self.linkStatusConnectionConfigurationCharacteristicChanged = TRUE;

        [self.lock unlock];

        // notify the subscribers that some values have changed
        [self updateSubscribers];

        // connect back to the peer to discover its services
        [self connectToPeer];
    } else if ([characteristic.UUID isEqual:
        [CBUUID UUIDWithString: GG_LINK_STATUS_CONNECTION_STATUS_CHARACTERISTIC_UUID]]) {
        GG_LOG_FINE("subscription to the Link Status Connection Status characteristic");
        self.linkStatusConnectionStatusSubscriber = central;
    } else if ([characteristic.UUID isEqual: [CBUUID UUIDWithString: GG_GATTLINK_TX_CHARACTERISTIC_UUID]]) {
        GG_LOG_FINE("subscription to Gattlink TX");

        // keep a reference to the subscriber
        self.gattlinkSubscriber = central;

        // update the MTU
        size_t mtu = central.maximumUpdateValueLength;
        GG_LOG_FINE("connection MTU: %d", (int)mtu);
        if (mtu > GG_STACK_TOOL_MAX_MTU) {
            GG_LOG_FINE("clamping MTU to %d", (int)GG_STACK_TOOL_MAX_MTU);
            mtu = GG_STACK_TOOL_MAX_MTU;
        }
        GG_StackToolBluetoothTransport_UpdateMtu(self.host, mtu);

        // we're now in a 'link connected' state
        GG_StackToolBluetoothTransport_NotifyConnected(self.host);
    }
}

// Delegate method called when a GATT client unsubscribes from one of our characteristics
-        (void)peripheralManager: (CBPeripheralManager *)peripheral
                         central: (CBCentral *)central
didUnsubscribeFromCharacteristic: (CBCharacteristic *)characteristic {
    GG_COMPILER_UNUSED(peripheral);
    GG_COMPILER_UNUSED(central);

    GG_LOG_FINER("characteristic un-subscription: %s", characteristic.UUID.UUIDString.UTF8String);

    if ([characteristic.UUID isEqual:
            [CBUUID UUIDWithString: GG_LINK_STATUS_CONNECTION_CONFIGURATION_CHARACTERISTIC_UUID]]) {
        GG_LOG_FINE("un-subscription from the Link Status Connection Configuration characteristic");

        // clear the subscriber if we have one
        self.linkStatusConnectionConfigurationSubscriber = nil;
    } else if ([characteristic.UUID isEqual:
        [CBUUID UUIDWithString: GG_LINK_STATUS_CONNECTION_STATUS_CHARACTERISTIC_UUID]]) {

        GG_LOG_FINE("un-subscription from the Link Status Connection Status characteristic");

        // clear the subscriber if we have one
        self.linkStatusConnectionStatusSubscriber = nil;
    } else if ([characteristic.UUID isEqual: [CBUUID UUIDWithString: GG_GATTLINK_TX_CHARACTERISTIC_UUID]]) {
        GG_LOG_FINE("un-subscription from Gattlink TX");

        // clear the subscriber if we have one
        self.gattlinkSubscriber = nil;
    }
}

// Delegate method called when a GATT client writes to one of our characteristics
- (void)peripheralManager: (CBPeripheralManager *)peripheral
  didReceiveWriteRequests: (NSArray<CBATTRequest *> *)requests {
    GG_COMPILER_UNUSED(peripheral);

    // sanity check
    if (requests.count != 1) {
        GG_LOG_WARNING("unexpected multiple write request (count = %u)", (int)requests.count);
        return;
    }

    CBATTRequest* request = requests[0];
    if ([request.characteristic.UUID isEqual: [CBUUID UUIDWithString: GG_GATTLINK_RX_CHARACTERISTIC_UUID]]) {
        // write to Gattlink RX
        GG_LOG_FINER(">>> gattlink RX, size=%u", (int)request.value.length);
        GG_StackToolBluetoothTransport_OnDataReceived(self.host, request.value.bytes, (size_t)request.value.length);
    }
}

// Delegate method called when a GATT client reads one of our characteristics
- (void)peripheralManager: (CBPeripheralManager *)peripheral
    didReceiveReadRequest: (CBATTRequest *)request {
    GG_LOG_FINER("received read request for %s", request.characteristic.UUID.UUIDString.UTF8String);

    if ([request.characteristic.UUID isEqual:
        [CBUUID UUIDWithString: GG_LINK_STATUS_CONNECTION_CONFIGURATION_CHARACTERISTIC_UUID]]) {
        request.value = self.linkStatusConnectionConfigurationCharacteristicValue;
        [peripheral respondToRequest: request withResult: CBATTErrorSuccess];
    } else if ([request.characteristic.UUID isEqual:
        [CBUUID UUIDWithString: GG_LINK_STATUS_CONNECTION_STATUS_CHARACTERISTIC_UUID]]) {
        request.value = self.linkStatusConnectionStatusCharacteristicValue;
        [peripheral respondToRequest: request withResult: CBATTErrorSuccess];
    }
}

// Delegate method called when it is Ok to notify subscribers
- (void)peripheralManagerIsReadyToUpdateSubscribers:(CBPeripheralManager *)peripheral {
    GG_COMPILER_UNUSED(peripheral);
    [self updateSubscribers];
}

// Delegate method called when a peripheral is connected
- (void)centralManager: (CBCentralManager *)central
  didConnectPeripheral: (CBPeripheral *)peripheral {
    GG_COMPILER_UNUSED(central);
    GG_COMPILER_UNUSED(peripheral);

    GG_LOG_INFO("connected!");

    // register as a delegate for the peripheral
    peripheral.delegate = self;

    // discover services
    [peripheral discoverServices: @[
        [CBUUID UUIDWithString: GG_LINK_CONFIGURATION_SERVICE_UUID]
    ]];
}

// Connect 'back' to the peer that connected to us in order to find its services.
// This isn't something that fits the Core Bluetooth API well.
// What we need to do here is ask for a list to 'connected' peripherals,
// and then request to connect to them (even though they're called 'connected'
// peripherals).
// Once connected, we can discover their services
- (void) connectToPeer {
    GG_LOG_INFO("connecting to peer");

    NSArray<CBPeripheral *>* peripherals =
        [self.centralManager retrieveConnectedPeripheralsWithServices: @[
            [CBUUID UUIDWithString: GG_LINK_CONFIGURATION_SERVICE_UUID]
        ]];

    // First, look for a peripheral with the same ID as our subscriber
    CBPeripheral* peer = NULL;
    for (CBPeripheral* peripheral in peripherals) {
        if (self.gattlinkSubscriber && [self.gattlinkSubscriber.identifier isEqual: peripheral.identifier]) {
            peer = peripheral;
            break;
        }
    }

    // If we haven't found a match, just use the first one in the list
    if (peer == NULL && peripherals.count) {
        peer = peripherals[0];
    }

    if (peer) {
        GG_LOG_FINE("connecting...");

        // keep a reference to the peripheral
        self.peer = peer;

        // connect
        [self.centralManager connectPeripheral: peer options: nil];
    } else {
        GG_LOG_WARNING("no peer found to connect to");
    }
}

// Update subscribers with any changed value
- (void) updateSubscribers {
    [self.lock lock];

    if (self.linkStatusConnectionConfigurationCharacteristicChanged &&
        self.linkStatusConnectionConfigurationSubscriber) {
        GG_LOG_FINE("updating subscribers of the Link Connection Configuration characteristic");
        BOOL result = [self.peripheralManager updateValue: self.linkStatusConnectionConfigurationCharacteristicValue
                                        forCharacteristic: self.linkStatusConnectionConfigurationCharacteristic
                                     onSubscribedCentrals: @[ self.linkStatusConnectionConfigurationSubscriber ]];
        if (result) {
            self.linkStatusConnectionConfigurationCharacteristicChanged = FALSE;
        } else {
            GG_LOG_FINE("update failed, will retry later");
        }
    } else if (self.linkStatusConnectionStatusCharacteristicChanged &&
               self.linkStatusConnectionStatusSubscriber) {
        GG_LOG_FINE("updating subscribers of the Link Connection Status characteristic");
        BOOL result = [self.peripheralManager updateValue: self.linkStatusConnectionStatusCharacteristicValue
                                        forCharacteristic: self.linkStatusConnectionStatusCharacteristic
                                     onSubscribedCentrals: @[ self.linkStatusConnectionStatusSubscriber ]];
        if (result) {
            self.linkStatusConnectionStatusCharacteristicChanged = FALSE;
        } else {
            GG_LOG_FINE("update failed, will retry later");
        }
    }

    // try to send
    self.txReady = TRUE;
    [self checkTxQueue];

    [self.lock unlock];
}

// Try to send data from the TX queue
// NOTE: the lock must be held
- (void)checkTxQueue {
    // do nothing if we're waiting for TX to be ready
    if (!self.txReady || !self.gattlinkSubscriber) {
        return;
    }

    // try to send all queued data
    while (self.txQueue.count) {
        // update the subscriber
        NSData* data = self.txQueue[self.txQueue.count - 1];
        BOOL result = [self.peripheralManager updateValue: data
                                        forCharacteristic: self.gattlinkTxCharacteristic
                                     onSubscribedCentrals: @[ self.gattlinkSubscriber ]];
        if (result) {
            // data sent, remove from the queue
            GG_LOG_FINER(">>> gattlink TX, size=%u", (int)data.length);
            [self.txQueue removeLastObject];
        } else {
            // couldn't send this one, just stop
            self.txReady = FALSE;
            break;
        }
    }
}

// Send data to Gattlink
- (void)sendData: (NSData *)data {
    if (self.gattlinkSubscriber == nil) {
        GG_LOG_WARNING("no subscriber, dropping gattlink TX data");
        return;
    }

    // lock before we check the queue
    [self.lock lock];

    // check if the queue is full
    if (self.txQueue.count >= GG_STACK_TOOL_TX_QUEUE_SIZE) {
        GG_LOG_WARNING("TX queue full, data dropped");
    } else {
        // add to the front of the queue
        [self.txQueue insertObject: data atIndex: 0];

        // send anything that's ready
        [self checkTxQueue];
    }

    [self.lock unlock];
}

// Release central resources that were obtained
- (void)cleanupCentral {
}

// Release peripheral resources that were obtained
- (void)cleanupPeripheral {
    // unpublish services
    if (self.peripheralManager) {
        [self.peripheralManager removeAllServices];
    }

    // release references
    self.gattlinkSubscriber                          = nil;
    self.linkStatusConnectionConfigurationSubscriber = nil;
    self.linkStatusConnectionStatusSubscriber        = nil;
}

@end

//----------------------------------------------------------------------
// GG_StackToolBluetoothTransport class (outer plain-C container)
//----------------------------------------------------------------------
struct GG_StackToolBluetoothTransport {
    GG_IMPLEMENTS(GG_DataSource);
    GG_IMPLEMENTS(GG_DataSink);

    GG_Loop*                loop;
    GG_EventListener*       connection_listener;
    GG_EventListener*       scan_listener;
    GG_EventListener*       mtu_listener;
    size_t                  mtu;
    GG_LoopDataSinkProxy*   sink_proxy;
    HubBluetoothTransport*  hub_driver;
    NodeBluetoothTransport* node_driver;
};

//----------------------------------------------------------------------
static GG_Result
GG_StackToolBluetoothTransport_SetDataSink(GG_DataSource* _self, GG_DataSink* sink)
{
    GG_StackToolBluetoothTransport* self = GG_SELF(GG_StackToolBluetoothTransport, GG_DataSource);

    // cleanup if we already have a sink proxy
    GG_LoopDataSinkProxy_Destroy(self->sink_proxy);
    self->sink_proxy = NULL;

    // do nothing if the sink is NULL
    if (!sink) {
        return GG_SUCCESS;
    }

    // create a proxy to deliver data to the sink on the loop thread
    GG_Result result = GG_Loop_CreateDataSinkProxy(self->loop,
                                                   GG_STACK_TOOL_RX_QUEUE_SIZE,
                                                   sink,
                                                   &self->sink_proxy);
    if (GG_FAILED(result)) {
        GG_LOG_SEVERE("GG_Loop_CreateDataSinkProxy failed (%d)", result);
        return result;
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
static GG_Result
GG_StackToolBluetoothTransport_PutData(GG_DataSink* _self, GG_Buffer* data, const GG_BufferMetadata* metadata)
{
    GG_StackToolBluetoothTransport* self = GG_SELF(GG_StackToolBluetoothTransport, GG_DataSink);
    GG_COMPILER_UNUSED(metadata);

    NSData* buffer = [NSData dataWithBytes: GG_Buffer_GetData(data) length: (NSUInteger)GG_Buffer_GetDataSize(data)];
    if (self->hub_driver) {
        [self->hub_driver sendData: buffer];
    } else {
        [self->node_driver sendData: buffer];
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
static GG_Result
GG_StackToolBluetoothTransport_SetListener(GG_DataSink* self, GG_DataSinkListener* listener)
{
    GG_COMPILER_UNUSED(self);
    GG_COMPILER_UNUSED(listener);

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_Result
GG_StackToolBluetoothTransport_Create(GG_Loop*                         loop,
                                      const char*                      device_id,
                                      GG_StackToolBluetoothTransport** transport)
{
    *transport = GG_AllocateZeroMemory(sizeof(GG_StackToolBluetoothTransport));
    if (*transport == NULL) {
        return GG_ERROR_OUT_OF_MEMORY;
    }

    // init basic fields
    (*transport)->loop = loop;
    (*transport)->mtu  = GG_STACK_TOOL_TX_DEFAULT_MTU;

    // set the function table
    GG_IMPLEMENT_INTERFACE(GG_StackToolBluetoothTransport, GG_DataSource) {
        .SetDataSink = GG_StackToolBluetoothTransport_SetDataSink
    };
    GG_SET_INTERFACE(*transport, GG_StackToolBluetoothTransport, GG_DataSource);
    GG_IMPLEMENT_INTERFACE(GG_StackToolBluetoothTransport, GG_DataSink) {
        .PutData     = GG_StackToolBluetoothTransport_PutData,
        .SetListener = GG_StackToolBluetoothTransport_SetListener
    };
    GG_SET_INTERFACE(*transport, GG_StackToolBluetoothTransport, GG_DataSink);

    // init the inner driver
    if (!strncmp(device_id, "node", 4)) {
        // run as a node
        const char* name;
        if (device_id[4] == ':') {
            name = device_id + 5;
        } else {
            name = GG_STACK_TOOL_DEFAULT_NODE_NAME;
        }
        NSString* advertisedName = [NSString stringWithUTF8String: name];
        (*transport)->node_driver = [[NodeBluetoothTransport alloc] initWithHost: *transport
                                                                  advertisedName: advertisedName];
    } else {
        // run as a hub
        (*transport)->hub_driver = [[HubBluetoothTransport alloc]
            initWithHost: *transport
                  target: [NSString stringWithUTF8String: device_id]];
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
void
GG_StackToolBluetoothTransport_Start(GG_StackToolBluetoothTransport* self)
{
    if (self->hub_driver) {
        [self->hub_driver start];
    } else {
        [self->node_driver start];
    }
}

//----------------------------------------------------------------------
void
GG_StackToolBluetoothTransport_Connect(GG_StackToolBluetoothTransport* self, const char* device_id)
{
    if (self->hub_driver) {
        NSString* target = [NSString stringWithUTF8String: device_id];
        if (target != NULL) {
            if ([target isEqualToString: @"scan"]) {
                self->hub_driver.target = nil;
            } else {
                self->hub_driver.target = [[NSUUID alloc] initWithUUIDString: target];
            }

            // disconnect if we're connected
            [self->hub_driver disconnect];
        }
    }
}

//----------------------------------------------------------------------
void
GG_StackToolBluetoothTransport_SetPreferredConnectionMode(GG_StackToolBluetoothTransport* self, uint8_t mode)
{
    if (self->hub_driver) {
        [self->hub_driver updatePreferredConnectionMode: mode];
    }
}

//----------------------------------------------------------------------
void
GG_StackToolBluetoothTransport_Destroy(GG_StackToolBluetoothTransport* self)
{
    if (self == NULL) return;

    GG_LoopDataSinkProxy_Destroy(self->sink_proxy);
    self->hub_driver = nil;
    self->node_driver = nil;

    GG_ClearAndFreeObject(self, 2);
}

//----------------------------------------------------------------------
GG_DataSource*
GG_StackToolBluetoothTransport_AsDataSource(GG_StackToolBluetoothTransport* self)
{
    return GG_CAST(self, GG_DataSource);
}

//----------------------------------------------------------------------
GG_DataSink*
GG_StackToolBluetoothTransport_AsDataSink(GG_StackToolBluetoothTransport* self)
{
    return GG_CAST(self, GG_DataSink);
}

//----------------------------------------------------------------------
void
GG_StackToolBluetoothTransport_SetMtuListener(GG_StackToolBluetoothTransport* self, GG_EventListener* listener)
{
    self->mtu_listener = listener;
}

//----------------------------------------------------------------------
void
GG_StackToolBluetoothTransport_SetScanListener(GG_StackToolBluetoothTransport* self, GG_EventListener* listener)
{
    self->scan_listener = listener;
}

//----------------------------------------------------------------------
void
GG_StackToolBluetoothTransport_SetConnectionListener(GG_StackToolBluetoothTransport* self, GG_EventListener* listener)
{
    self->connection_listener = listener;
}

//----------------------------------------------------------------------
static int
GG_StackToolBluetoothTransport_UpdateMtu_(void* args)
{
    GG_StackToolBluetoothTransport* self = (GG_StackToolBluetoothTransport*)args;

    GG_StackLinkMtuChangeEvent mtu_changed_event = {
        .base = {
            .type = GG_EVENT_TYPE_LINK_MTU_CHANGE
        },
        .link_mtu = (unsigned int)self->mtu
    };

    GG_EventListener_OnEvent(self->mtu_listener, &mtu_changed_event.base);

    return 0;
}

//----------------------------------------------------------------------
static void
GG_StackToolBluetoothTransport_UpdateMtu(GG_StackToolBluetoothTransport* self, size_t mtu)
{
    if (mtu > self->mtu) {
        GG_LOG_INFO("new MTU: %u", (int)mtu);
        self->mtu = mtu;

        if (self->mtu_listener) {
            // emit an event on the loop thread
            GG_Loop_InvokeSync(self->loop, GG_StackToolBluetoothTransport_UpdateMtu_, self, NULL);
        }
    }
}

//----------------------------------------------------------------------
static int
GG_StackToolBluetoothTransport_NotifyConnected_(void* args)
{
    GG_StackToolBluetoothTransport* self = (GG_StackToolBluetoothTransport*)args;

    GG_Event connected_event = {
        .type = GG_EVENT_TYPE_BLUETOOTH_LINK_CONNECTED_EVENT
    };

    GG_EventListener_OnEvent(self->connection_listener, &connected_event);

    return 0;
}

//----------------------------------------------------------------------
static void
GG_StackToolBluetoothTransport_NotifyConnected(GG_StackToolBluetoothTransport* self)
{
    GG_LOG_INFO("~~~ link connected ~~~");
    if (self->connection_listener) {
        // emit an event on the loop thread
        GG_Loop_InvokeSync(self->loop, GG_StackToolBluetoothTransport_NotifyConnected_, self, NULL);
    }
}

//----------------------------------------------------------------------
static void
GG_StackToolBluetoothTransport_OnDataReceived(GG_StackToolBluetoothTransport* self,
                                              const void*                     data,
                                              size_t                          data_size)
{
    if (!self->sink_proxy) {
        GG_LOG_WARNING("received data without sink, dropping");
        return;
    }

    // send the data to the sink via the proxy, ignoring any error
    GG_DynamicBuffer* buffer;
    GG_Result result = GG_DynamicBuffer_Create(data_size, &buffer);
    GG_ASSERT(GG_SUCCEEDED(result));
    GG_DynamicBuffer_SetData(buffer, data, data_size);
    GG_DataSink_PutData(GG_LoopDataSinkProxy_AsDataSink(self->sink_proxy), GG_DynamicBuffer_AsBuffer(buffer), NULL);
    GG_DynamicBuffer_Release(buffer);
}

//----------------------------------------------------------------------
typedef struct {
    GG_StackToolBluetoothTransport*         self;
    GG_StackToolBluetoothTransportScanEvent event;
} OnDeviceDiscoveredArgs;

static int
GG_StackToolBluetoothTransport_OnDeviceDiscovered_(void* _args)
{
    OnDeviceDiscoveredArgs* args = (OnDeviceDiscoveredArgs*)_args;

    // notify the listener
    GG_EventListener_OnEvent(args->self->scan_listener, &args->event.base);

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
static void
GG_StackToolBluetoothTransport_OnDeviceDiscovered(GG_StackToolBluetoothTransport* self,
                                                  const char*                     peripheral_name,
                                                  const char*                     peripheral_id,
                                                  int                             rssi)
{
    // check if we have a listener
    if (!self->scan_listener) {
        return;
    }

    OnDeviceDiscoveredArgs args = {
        .self = self,
        .event = {
            .base = {
                .type = GG_EVENT_TYPE_BLUETOOTH_TRANSPORT_SCAN
            },
            .peripheral_name = peripheral_name,
            .peripheral_id   = peripheral_id,
            .rssi            = rssi
        }
    };

    GG_Loop_InvokeSync(self->loop, GG_StackToolBluetoothTransport_OnDeviceDiscovered_, &args, NULL);
}

//----------------------------------------------------------------------
typedef struct {
    GG_StackToolBluetoothTransport*                               self;
    GG_StackToolBluetoothTransportLinkStatusConnectionConfigEvent event;
} OnLinkStatusConfigurationUpdatedArgs;

static int
GG_StackToolBluetoothTransport_OnLinkStatusConfigurationUpdated_(void* _args)
{
    OnLinkStatusConfigurationUpdatedArgs* args = (OnLinkStatusConfigurationUpdatedArgs*)_args;

    // notify the listener
    GG_EventListener_OnEvent(args->self->connection_listener, &args->event.base);

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
static void
GG_StackToolBluetoothTransport_OnLinkStatusConfigurationUpdated(GG_StackToolBluetoothTransport* self,
                                                                unsigned int connection_interval,
                                                                unsigned int slave_latency,
                                                                unsigned int supervision_timeout,
                                                                unsigned int mtu,
                                                                unsigned int mode)
{
    // check if we have a listener
    if (!self->connection_listener) {
        return;
    }

    // notify the listener
    OnLinkStatusConfigurationUpdatedArgs args = {
        .self = self,
        .event = {
            .base = {
                .type = GG_EVENT_TYPE_BLUETOOTH_LINK_STATUS_CONENCTION_CONFIG_EVENT
            },
            .connection_interval = connection_interval,
            .slave_latency       = slave_latency,
            .supervision_timeout = supervision_timeout,
            .mtu                 = mtu,
            .mode                = mode
        }
    };

    GG_Loop_InvokeSync(self->loop, GG_StackToolBluetoothTransport_OnLinkStatusConfigurationUpdated_, &args, NULL);
}

//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  BluetoothConfiguration+Defaults.swift
//  GoldenGateHost
//
//  Created by Emanuel Jarnea on 20/07/2020.
//

import BluetoothConnection
import CoreBluetooth

#if os(iOS)
import UIKit
#endif

extension BluetoothConfiguration.LinkStatusServiceConfiguration {
    static let `default` = Self(
        serviceUUID: CBUUID(string: "ABBAFD00-E56A-484C-B832-8B17CF6CBFE8"),
        currentConnectionConfigurationUUID: CBUUID(string: "ABBAFD01-E56A-484C-B832-8B17CF6CBFE8"),
        currentConnectionStatusUUID: CBUUID(string: "ABBAFD02-E56A-484C-B832-8B17CF6CBFE8"),
        bondSecureUUID: CBUUID(string: "ABBAFD03-E56A-484C-B832-8B17CF6CBFE8")
    )
}

extension BluetoothConfiguration.LinkServiceConfiguration {
    /// The original, but now deprecated, 128-bit UUID configuration
    static let deprecated = Self(
        serviceUUID: CBUUID(string: "ABBAFF00-E56A-484C-B832-8B17CF6CBFE8"),
        rxUUID: CBUUID(string: "ABBAFF01-E56A-484C-B832-8B17CF6CBFE8"),
        txUUID: CBUUID(string: "ABBAFF02-E56A-484C-B832-8B17CF6CBFE8")
    )

    /// The shiny new, 16-bit UUID configuration
    static let `default` = Self(
        serviceUUID: CBUUID(string: "FD62"),
        rxUUID: CBUUID(string: "ABBAFF01-E56A-484C-B832-8B17CF6CBFE8"),
        txUUID: CBUUID(string: "ABBAFF02-E56A-484C-B832-8B17CF6CBFE8")
    )
}

extension BluetoothConfiguration.LinkConfigurationServiceConfiguration {
    static let `default` = Self(
        serviceUUID: CBUUID(string: "ABBAFC00-E56A-484C-B832-8B17CF6CBFE8"),
        preferredConnectionConfigurationUUID: CBUUID(string: "ABBAFC01-E56A-484C-B832-8B17CF6CBFE8"),
        preferredConnectionModeUUID: CBUUID(string: "ABBAFC02-E56A-484C-B832-8B17CF6CBFE8"),
        generalPurposeCommandUUID: CBUUID(string: "ABBAFC03-E56A-484C-B832-8B17CF6CBFE8")
    )
}

extension BluetoothConfiguration.DeviceInfoServiceConfiguration {
    static let `default` = Self(
        serviceUUID: CBUUID(string: "180A"),
        modelNumberUUID: CBUUID(string: "2A24"),
        serialNumberUUID: CBUUID(string: "2A25"),
        firmwareRevisionUUID: CBUUID(string: "2A26"),
        hardwareRevisionUUID: CBUUID(string: "2A27"),
        softwareRevisionUUID: CBUUID(string: "2A28")
    )
}

extension BluetoothConfiguration.ConfirmationServiceConfiguration {
    static let `default` = Self(
        serviceUUID: CBUUID(string: "AC2F0045-8182-4BE5-91E0-2992E6B40EBB"),
        ephemeralCharacteristicPointerUUID: CBUUID(string: "AC2F0145-8182-4BE5-91E0-2992E6B40EBB")
    )
}

extension BluetoothConfiguration {
    /// Default configuration
    static let `default` = Self(
        name: {
            #if os(iOS)
                return UIDevice.current.name
            #else
                return Host.current().localizedName ?? "macOS Device"
            #endif
        }(),
        linkStatusService: LinkStatusServiceConfiguration.default,
        linkService: [LinkServiceConfiguration.default, .deprecated],
        linkConfigurationService: LinkConfigurationServiceConfiguration.default,
        deviceInfoService: DeviceInfoServiceConfiguration.default,
        confirmationService: ConfirmationServiceConfiguration.default
    )
}

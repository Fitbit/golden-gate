//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  BluetoothConfiguration.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 10/30/17.
//

import CoreBluetooth
import Foundation

#if os(iOS)
import UIKit
#endif

public struct BluetoothConfiguration {
    let name: String

    struct LinkStatusServiceConfiguration {
        let serviceUUID: CBUUID
        let currentConnectionConfigurationUUID: CBUUID
        let currentConnectionStatusUUID: CBUUID
        let bondSecureUUID: CBUUID

        static let `default` = LinkStatusServiceConfiguration(
            serviceUUID: CBUUID(string: "ABBAFD00-E56A-484C-B832-8B17CF6CBFE8"),
            currentConnectionConfigurationUUID: CBUUID(string: "ABBAFD01-E56A-484C-B832-8B17CF6CBFE8"),
            currentConnectionStatusUUID: CBUUID(string: "ABBAFD02-E56A-484C-B832-8B17CF6CBFE8"),
            bondSecureUUID: CBUUID(string: "ABBAFD03-E56A-484C-B832-8B17CF6CBFE8")
        )
    }

    let linkStatusService: LinkStatusServiceConfiguration

    struct GattlinkServiceConfiguration {
        let serviceUUID: CBUUID
        let rxUUID: CBUUID
        let txUUID: CBUUID

        static let `default` = GattlinkServiceConfiguration(
            serviceUUID: CBUUID(string: "ABBAFF00-E56A-484C-B832-8B17CF6CBFE8"),
            rxUUID: CBUUID(string: "ABBAFF01-E56A-484C-B832-8B17CF6CBFE8"),
            txUUID: CBUUID(string: "ABBAFF02-E56A-484C-B832-8B17CF6CBFE8")
        )
    }

    let gattlinkService: GattlinkServiceConfiguration

    struct LinkConfigurationServiceConfiguration {
        let serviceUUID: CBUUID
        let preferredConnectionConfigurationUUID: CBUUID
        let preferredConnectionModeUUID: CBUUID
        let generalPurposeCommandUUID: CBUUID

        static let `default` = LinkConfigurationServiceConfiguration(
            serviceUUID: CBUUID(string: "ABBAFC00-E56A-484C-B832-8B17CF6CBFE8"),
            preferredConnectionConfigurationUUID: CBUUID(string: "ABBAFC01-E56A-484C-B832-8B17CF6CBFE8"),
            preferredConnectionModeUUID: CBUUID(string: "ABBAFC02-E56A-484C-B832-8B17CF6CBFE8"),
            generalPurposeCommandUUID: CBUUID(string: "ABBAFC03-E56A-484C-B832-8B17CF6CBFE8")
        )
    }

    let linkConfigurationService: LinkConfigurationServiceConfiguration

    /// Device Info Service exposes vendor information about a device.
    public struct DeviceInfoServiceConfiguration {
        public let serviceUUID: CBUUID
        public let serialNumberUUID: CBUUID
        public let modelNumberUUID: CBUUID

        static let `default` = DeviceInfoServiceConfiguration(
            serviceUUID: CBUUID(string: "180A"),
            serialNumberUUID: CBUUID(string: "2A25"),
            modelNumberUUID: CBUUID(string: "2A24")
        )
    }

    public let deviceInfoService: DeviceInfoServiceConfiguration

    /// Battery Level Service exposes a device battery level.
    public struct BatteryLevelServiceConfiguration {
        public let serviceUUID: CBUUID
        public let batteryLevelUUID: CBUUID

        static let `default` = BatteryLevelServiceConfiguration(
            serviceUUID: CBUUID(string: "180F"),
            batteryLevelUUID: CBUUID(string: "2A19")
        )
    }

    public let batteryLevelService: BatteryLevelServiceConfiguration

    public struct ConfirmationServiceConfiguration {
        public let serviceUUID: CBUUID
        public let ephemeralCharacteristicPointerUUID: CBUUID

        static let `default` = ConfirmationServiceConfiguration(
            serviceUUID: CBUUID(string: "AC2F0045-8182-4BE5-91E0-2992E6B40EBB"),
            ephemeralCharacteristicPointerUUID: CBUUID(string: "AC2F0145-8182-4BE5-91E0-2992E6B40EBB")
        )
    }

    public let confirmationService: ConfirmationServiceConfiguration

    /// A non-GG legacy services match.
    public let legacyServiceUUIDMatcher: (CBUUID) -> Bool

    /// Default configuration
    public static let `default` = BluetoothConfiguration(
        name: {
            #if os(iOS)
                return UIDevice.current.name
            #else
                return Host.current().localizedName ?? "macOS Device"
            #endif
        }(),
        linkStatusService: LinkStatusServiceConfiguration.default,
        gattlinkService: GattlinkServiceConfiguration.default,
        linkConfigurationService: LinkConfigurationServiceConfiguration.default,
        deviceInfoService: DeviceInfoServiceConfiguration.default,
        batteryLevelService: BatteryLevelServiceConfiguration.default,
        confirmationService: ConfirmationServiceConfiguration.default,
        legacyServiceUUIDMatcher: {
            return $0.uuidString.hasPrefix("ADAB") && $0.uuidString.hasSuffix("-6E7D-4601-BDA2-BFFAA68956BA")
        }
    )
}

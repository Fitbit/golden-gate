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

public struct BluetoothConfiguration {
    public let name: String

    public let linkStatusService: LinkStatusServiceConfiguration

    /// UUID configurations supported by the link service
    public let linkService: [LinkServiceConfiguration]

    public let linkConfigurationService: LinkConfigurationServiceConfiguration

    public let deviceInfoService: DeviceInfoServiceConfiguration

    public let confirmationService: ConfirmationServiceConfiguration

    public init(
        name: String,
        linkStatusService: LinkStatusServiceConfiguration,
        linkService: [LinkServiceConfiguration],
        linkConfigurationService: LinkConfigurationServiceConfiguration,
        deviceInfoService: DeviceInfoServiceConfiguration,
        confirmationService: ConfirmationServiceConfiguration
    ) {
        self.name = name
        self.linkStatusService = linkStatusService
        self.linkService = linkService
        self.linkConfigurationService = linkConfigurationService
        self.deviceInfoService = deviceInfoService
        self.confirmationService = confirmationService
    }
}

extension BluetoothConfiguration {
    public struct LinkStatusServiceConfiguration {
        public let serviceUUID: CBUUID
        public let currentConnectionConfigurationUUID: CBUUID
        public let currentConnectionStatusUUID: CBUUID
        public let bondSecureUUID: CBUUID

        public init(
            serviceUUID: CBUUID,
            currentConnectionConfigurationUUID: CBUUID,
            currentConnectionStatusUUID: CBUUID,
            bondSecureUUID: CBUUID
        ) {
            self.serviceUUID = serviceUUID
            self.currentConnectionConfigurationUUID = currentConnectionConfigurationUUID
            self.currentConnectionStatusUUID = currentConnectionStatusUUID
            self.bondSecureUUID = bondSecureUUID
        }
    }

    public struct LinkServiceConfiguration {
        public let serviceUUID: CBUUID
        public let rxUUID: CBUUID
        public let txUUID: CBUUID

        public init(serviceUUID: CBUUID, rxUUID: CBUUID, txUUID: CBUUID) {
            self.serviceUUID = serviceUUID
            self.rxUUID = rxUUID
            self.txUUID = txUUID
        }
    }

    public struct LinkConfigurationServiceConfiguration {
       public let serviceUUID: CBUUID
       public let preferredConnectionConfigurationUUID: CBUUID
       public let preferredConnectionModeUUID: CBUUID
       public let generalPurposeCommandUUID: CBUUID

       public init(
           serviceUUID: CBUUID,
           preferredConnectionConfigurationUUID: CBUUID,
           preferredConnectionModeUUID: CBUUID,
           generalPurposeCommandUUID: CBUUID
       ) {
           self.serviceUUID = serviceUUID
           self.preferredConnectionConfigurationUUID = preferredConnectionConfigurationUUID
           self.preferredConnectionModeUUID = preferredConnectionModeUUID
           self.generalPurposeCommandUUID = generalPurposeCommandUUID
       }
   }

    /// Device Info Service exposes vendor information about a device.
    public struct DeviceInfoServiceConfiguration {
        public let serviceUUID: CBUUID
        public let modelNumberUUID: CBUUID
        public let serialNumberUUID: CBUUID
        public let firmwareRevisionUUID: CBUUID
        public let hardwareRevisionUUID: CBUUID
        public let softwareRevisionUUID: CBUUID

        public init(
            serviceUUID: CBUUID,
            modelNumberUUID: CBUUID,
            serialNumberUUID: CBUUID,
            firmwareRevisionUUID: CBUUID,
            hardwareRevisionUUID: CBUUID,
            softwareRevisionUUID: CBUUID
        ) {
            self.serviceUUID = serviceUUID
            self.modelNumberUUID = modelNumberUUID
            self.serialNumberUUID = serialNumberUUID
            self.firmwareRevisionUUID = firmwareRevisionUUID
            self.hardwareRevisionUUID = hardwareRevisionUUID
            self.softwareRevisionUUID = softwareRevisionUUID
        }
    }

    public struct ConfirmationServiceConfiguration {
        public let serviceUUID: CBUUID
        public let ephemeralCharacteristicPointerUUID: CBUUID

        public init(serviceUUID: CBUUID, ephemeralCharacteristicPointerUUID: CBUUID) {
            self.serviceUUID = serviceUUID
            self.ephemeralCharacteristicPointerUUID = ephemeralCharacteristicPointerUUID
        }
    }
}

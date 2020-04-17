//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  DiscoveredBluetoothPeer.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 11/6/17.
//

import CoreBluetooth
import Foundation
import RxBluetoothKit
import RxCocoa
import RxSwift

public class DiscoveredBluetoothPeer {
    internal let peripheral: Peripheral
    public let advertisementData: BehaviorRelay<AdvertisementData?>
    public let rssi: BehaviorRelay<Int>
    public let name: BehaviorRelay<String>

    public var identifier: UUID {
        return peripheral.identifier
    }

    public var services: [CBUUID]? {
        return peripheral.services?.map { $0.uuid } ?? advertisementData.value?.serviceUUIDs
    }

    init(peripheral: Peripheral, advertisementData: AdvertisementData?, rssi: NSNumber?) {
        self.peripheral = peripheral
        self.advertisementData = BehaviorRelay(value: advertisementData)
        self.rssi = BehaviorRelay(value: type(of: self).parse(rssi: rssi) ?? Int(Int8.min))
        self.name = BehaviorRelay(value: advertisementData?.localName ?? peripheral.name ?? peripheral.identifier.uuidString)
    }

    convenience init(_ scannedPeripheral: ScannedPeripheral) {
        self.init(
            peripheral: scannedPeripheral.peripheral,
            advertisementData: scannedPeripheral.advertisementData,
            rssi: scannedPeripheral.rssi
        )
    }

    func merge(from scannedPeripheral: ScannedPeripheral) {
        advertisementData.accept(scannedPeripheral.advertisementData)

        if let newRssi = type(of: self).parse(rssi: scannedPeripheral.rssi) {
            rssi.accept(newRssi)
        }

        if let newName = scannedPeripheral.advertisementData.localName {
            name.accept(newName)
        }
    }

    func update(newRssi: NSNumber) {
        if let newRssi = type(of: self).parse(rssi: newRssi) {
            rssi.accept(newRssi)
        }
    }

    /// Parser for RSSI readings where faulty readings are indicated by 127.
    private static func parse(rssi: NSNumber? = nil) -> Int? {
        guard let rssi = rssi else { return nil }

        return rssi.intValue == Int8.max ? nil : rssi.intValue
    }
}

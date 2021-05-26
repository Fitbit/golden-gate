//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  ScannedPeripheralType.swift
//  BluetoothConnection
//
//  Created by Emanuel Jarnea on 18/11/2020.
//

import Foundation
import RxBluetoothKit
import RxSwift

/// Represents a scanned peripheral - containing it's advertisment data, rssi and peripheral itself.
/// To perform further actions `peripheral` instance can be used ia. to maintain connection.
public protocol ScannedPeripheralType: AnyObject {
    /// A peripheral instance, that allows to perform further bluetooth actions.
    func peripheral() -> PeripheralType

    /// Advertisement data of scanned peripheral
    var advertisementData: AdvertisementData { get }

    /// Scanned peripheral's RSSI value.
    var rssi: NSNumber { get }
}

extension ScannedPeripheral: ScannedPeripheralType {
    public func peripheral() -> PeripheralType { peripheral }
}

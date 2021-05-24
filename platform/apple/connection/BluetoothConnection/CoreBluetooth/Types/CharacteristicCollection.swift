//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  CharacteristicCollection.swift
//  BluetoothConnection
//
//  Created by Vlad Corneci on 10/08/2020.
//

import CoreBluetooth
import RxBluetoothKit

/// A queryable set of Bluetooth characteristics.
public struct CharacteristicCollection {
    private let characteristics: [CharacteristicType]

    public init(_ characteristics: [CharacteristicType] = []) {
        self.characteristics = characteristics
    }

    public func optional(_ uuid: CBUUID) -> CharacteristicType? {
        guard let characteristic = characteristics.first(where: { $0.uuid == uuid }) else {
            LogWarning("Optional characteristic not found: \(uuid)", domain: .bluetooth)
            return nil
        }

        return characteristic
    }

    public func required(_ uuid: CBUUID) throws -> CharacteristicType {
        guard let characteristic = characteristics.first(where: { $0.uuid == uuid }) else {
            LogError("Required characteristic not found: \(uuid)", domain: .bluetooth)
            throw PeripheralError.missingCharacteristic(uuid)
        }

        return characteristic
    }
}

extension Characteristic: CustomStringConvertible {
    public var description: String {
        return "\(self.characteristic)"
    }
}

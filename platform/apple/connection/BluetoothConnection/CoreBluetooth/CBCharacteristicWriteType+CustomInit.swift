//
//  CBCharacteristicWriteType+CustomInit.swift
//  BluetoothConnection
//
//  Created by Vlad Corneci on 02/09/2020.
//  Copyright © 2020 Fitbit. All rights reserved.
//

import CoreBluetooth

public extension CBCharacteristicWriteType {
    init?(fastestFor properties: CBCharacteristicProperties) {
        if properties.contains(.writeWithoutResponse) {
            self = .withoutResponse
        } else if properties.contains(.write) {
            self = .withResponse
        } else {
            return nil
        }
    }
}

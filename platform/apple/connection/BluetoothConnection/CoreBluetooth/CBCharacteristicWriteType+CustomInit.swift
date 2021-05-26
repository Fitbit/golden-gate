//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  CBCharacteristicWriteType+CustomInit.swift
//  BluetoothConnection
//
//  Created by Vlad Corneci on 02/09/2020.
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

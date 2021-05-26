//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  Hub+NetworkLink.swift
//  GoldenGate
//
//  Created by Sylvain Rebaud on 2/25/19.
//

import BluetoothConnection
import CoreBluetooth
import RxBluetoothKit
import RxRelay
import RxSwift

extension Hub {
    /// Creates a network link for the given characteristics.
    ///
    /// - Parameters:
    ///   - rxCharacteristic: The Node characteristic that receives data from the Hub.
    ///   - txCharacteristic: The Node characteristic that transmits data to the Hub.
    func networkLink(
        rxCharacteristic: CharacteristicType,
        txCharacteristic: CharacteristicType
    ) throws -> NetworkLink {
        // Discover the fastest communication mechanism (Write or WriteWithoutResponse)
        // and throw if we can't find any characteristics writable at all.
        guard let writeType = CBCharacteristicWriteType(fastestFor: rxCharacteristic.properties) else {
            throw PeripheralError.unexpectedCharacteristicProperties(rxCharacteristic.uuid)
        }

        let dataSink = CentralManagerSink(
            listenerScheduler: protocolScheduler,
            characteristic: rxCharacteristic
        )

        let dataSource = CentralManagerSource(
            sinkScheduler: protocolScheduler,
            characteristic: txCharacteristic
        )

        return NetworkLink(
            dataSink: dataSink,
            dataSource: dataSource,
            mtu: .init(value: UInt(rxCharacteristic.service().peripheral().maximumWriteValueLength(for: writeType)))
        )
    }
}

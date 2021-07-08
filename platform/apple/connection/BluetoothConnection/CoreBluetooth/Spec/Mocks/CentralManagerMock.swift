//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  CentralManagerMock.swift
//  BluetoothConnectionTests
//
//  Created by Emanuel Jarnea on 19/11/2020.
//

import BluetoothConnection
import CoreBluetooth
import RxBluetoothKit
import RxSwift

final class CentralManagerMock: CentralManagerType {
    var connectedPeripherals: [PeripheralType] = []
    var connectedServiceUUIDsFilter: [CBUUID]?
    func retrieveConnectedPeripherals(withServices serviceUUIDs: [CBUUID]) -> [PeripheralType] {
        connectedServiceUUIDsFilter = serviceUUIDs
        return connectedPeripherals
    }

    var peripherals: [PeripheralType] = []
    func retrievePeripherals(withIdentifiers identifiers: [UUID]) -> [PeripheralType] {
        peripherals
    }

    let scannedPeripherals = PublishSubject<ScannedPeripheralType>()
    var scannedServiceUUIDsFilter: [CBUUID]?
    func scanForPeripherals(withServices serviceUUIDs: [CBUUID]?, options: [String: Any]?) -> Observable<ScannedPeripheralType> {
        scannedPeripherals.asObservable()
            .do(onSubscribe: { self.scannedServiceUUIDsFilter = serviceUUIDs })
    }

    let bluetoothState = ReplaySubject<BluetoothState>.create(bufferSize: 1)
    func stabilizedState() -> Observable<BluetoothState> {
        bluetoothState.asObservable()
    }

    let connectionStatus = PublishSubject<ConnectionStatus<PeripheralType>>()
    func establishConnection(identifier: UUID, scheduler: SchedulerType) -> Observable<ConnectionStatus<PeripheralType>> {
        connectionStatus.asObservable()
    }
}

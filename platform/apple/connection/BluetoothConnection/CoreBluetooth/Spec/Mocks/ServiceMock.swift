//
//  ServiceMock.swift
//  BluetoothConnectionTests
//
//  Created by Emanuel Jarnea on 19/11/2020.
//  Copyright © 2020 Fitbit. All rights reserved.
//

import BluetoothConnection
import CoreBluetooth
import RxBluetoothKit
import RxSwift

final class ServiceMock: ServiceType {
    var uuid = CBUUID()

    var characteristicList: [CharacteristicType]?
    func characteristics() -> [CharacteristicType]? {
        characteristicList
    }

    private unowned let _peripheral: PeripheralType
    func peripheral() -> PeripheralType {
        _peripheral
    }

    let discoveredCharacteristics = PublishSubject<[CharacteristicType]>()
    private(set) var didDiscoverAllCharacteristics: Bool?
    func discoverCharacteristics(_ characteristicUUIDs: [CBUUID]?) -> Single<[CharacteristicType]> {
        discoveredCharacteristics.take(1).asSingle()
            .do(onSubscribe: { self.didDiscoverAllCharacteristics = characteristicUUIDs == nil })
    }

    init(peripheral: PeripheralType) {
        self._peripheral = peripheral
    }
}

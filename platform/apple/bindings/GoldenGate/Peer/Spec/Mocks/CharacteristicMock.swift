//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  CharacteristicMock.swift
//  GoldenGateTests
//
//  Created by Emanuel Jarnea on 19/11/2020.
//

import BluetoothConnection
import CoreBluetooth
import RxBluetoothKit
import RxSwift

final class CharacteristicMock: CharacteristicType {
    let uuid: CBUUID

    private unowned let _service: ServiceType
    func service() -> ServiceType {
        _service
    }

    var properties = CBCharacteristicProperties([.read, .write])

    private(set) var didAccessSecurely = false
    func secureAccess() -> Completable {
        Completable.empty()
            .do(onSubscribe: { self.didAccessSecurely = true })
    }

    var value: Data?
    func readValue() -> Single<CharacteristicType> {
        .just(self)
    }

    let didUpdateValueSubject = PublishSubject<Void>()
    func observeValueUpdateAndSetNotification() -> Observable<CharacteristicType> {
        didUpdateValueSubject
            .map { _ in self }
    }

    private(set) var lastWrittenData: Data?
    private(set) var lastWriteType: CBCharacteristicWriteType?
    func writeValue(_ data: Data, type: CBCharacteristicWriteType) -> Single<CharacteristicType> {
        Single.just(self)
            .do(onSubscribe: {
                self.lastWrittenData = data
                self.lastWriteType = type
            })
    }

    init(uuid: CBUUID = CBUUID(), service: ServiceType, value: Data? = nil) {
        self.uuid = uuid
        self._service = service
        self.value = value
    }
}

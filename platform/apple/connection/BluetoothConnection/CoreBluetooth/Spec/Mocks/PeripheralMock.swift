//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  PeripheralMock.swift
//  BluetoothConnectionTests
//
//  Created by Emanuel Jarnea on 19/11/2020.
//

import BluetoothConnection
import CoreBluetooth
import RxBluetoothKit
import RxSwift

final class PeripheralMock: PeripheralType {
    var name: String?
    var identifier = UUID()

    func attach() {}

    var servicesList: [ServiceType]?
    func services() -> [ServiceType]? { servicesList }

    var isConnected = false

    let isANCSAuthorizedSubject = PublishSubject<Bool>()
    var isANCSAuthorized: Observable<Bool> { isANCSAuthorizedSubject.asObservable() }

    let connectablePeripheral = PublishSubject<Void>()
    private(set) var didEstablishConnection: Bool = false
    func establishConnection(options: [String: Any]?) -> Observable<PeripheralType> {
        connectablePeripheral.asObservable()
            .map { _ in self }
            .do(onSubscribe: { self.didEstablishConnection = true })
    }

    let discoveredServices = PublishSubject<[ServiceType]>()
    private(set) var didDiscoverServices: Bool?
    private(set) var servicesToDiscover: [CBUUID]?
    func discoverServices(_ serviceUUIDs: [CBUUID]?) -> Single<[ServiceType]> {
        discoveredServices.take(1).asSingle()
            .do(onSubscribe: {
                self.didDiscoverServices = true
                self.servicesToDiscover = serviceUUIDs
            })
    }

    let servicesModification = PublishSubject<[ServiceType]>()
    func observeServicesModification() -> Observable<(PeripheralType, [ServiceType])> {
        servicesModification.map { (self, $0) }
    }

    var maxWriteValueLength = 0
    func maximumWriteValueLength(for type: CBCharacteristicWriteType) -> Int {
        maxWriteValueLength
    }

    private(set) var writtenData: Data?
    func writeValue(_ data: Data, for characteristic: CharacteristicType, type: CBCharacteristicWriteType, canSendWriteWithoutResponseCheckEnabled: Bool) -> Single<CharacteristicType> {
        Single.just(characteristic)
            .do(onSubscribe: { self.writtenData = data })
    }

    var rssi = PublishSubject<Int>()
    func readRSSI() -> Single<(PeripheralType, Int)> {
        rssi.map { (self, $0) }
            .take(1)
            .asSingle()
    }
}

extension PeripheralMock {
    convenience init(identifier: UUID) {
        self.init()
        self.identifier = identifier
    }
}

final class ScannedPeripheralMock: ScannedPeripheralType {
    init(peripheral: PeripheralType, rssi: NSNumber = 0) {
        self._peripheral = peripheral
        self.rssi = rssi
    }

    private let _peripheral: PeripheralType
    func peripheral() -> PeripheralType {
        _peripheral
    }

    var advertisementData = AdvertisementData(advertisementData: [:])
    var rssi: NSNumber
}

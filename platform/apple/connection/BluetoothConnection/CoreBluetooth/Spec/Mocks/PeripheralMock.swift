//
//  PeripheralMock.swift
//  BluetoothConnectionTests
//
//  Created by Emanuel Jarnea on 19/11/2020.
//  Copyright © 2020 Fitbit. All rights reserved.
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

    let connectablePeripheral = PublishSubject<PeripheralType>()
    func establishConnection(options: [String : Any]?) -> Observable<PeripheralType> {
        connectablePeripheral.asObservable()
    }

    let discoveredServices = PublishSubject<[ServiceType]>()
    private(set) var didDiscoverAllServices: Bool?
    func discoverServices(_ serviceUUIDs: [CBUUID]?) -> Single<[ServiceType]> {
        discoveredServices.take(1).asSingle()
            .do(onSubscribe: { self.didDiscoverAllServices = serviceUUIDs == nil })
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

    var rssi: (PeripheralType, Int)?
    func readRSSI() -> Single<(PeripheralType, Int)> {
        rssi.map(Single.just) ?? Single.never()
    }
}

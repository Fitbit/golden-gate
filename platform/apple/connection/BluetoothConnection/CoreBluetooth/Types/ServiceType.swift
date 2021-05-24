//
//  ServiceType.swift
//  BluetoothConnection
//
//  Created by Emanuel Jarnea on 18/11/2020.
//  Copyright © 2020 Fitbit. All rights reserved.
//

import CoreBluetooth
import Foundation
import RxBluetoothKit
import RxSwift

/// ServiceType is a ReactiveX wrapper over Core Bluetooth's `CBService`
/// https://developer.apple.com/documentation/corebluetooth/cbservice
public protocol ServiceType: AnyObject {
    /// Service's UUID
    var uuid: CBUUID { get }

    /// Service's characteristics
    func characteristics () -> [CharacteristicType]?

    /// Peripheral to which this service belongs
    func peripheral() -> PeripheralType

    /// Function that triggers characteristics discovery for specified Services and identifiers.
    func discoverCharacteristics(_ characteristicUUIDs: [CBUUID]?) -> Single<[CharacteristicType]>
}

extension Service: ServiceType {
    public func characteristics() -> [CharacteristicType]? { characteristics }

    public func peripheral() -> PeripheralType { peripheral }

    public func discoverCharacteristics(_ characteristicUUIDs: [CBUUID]?) -> Single<[CharacteristicType]> {
        let characteristics: Single<[Characteristic]> = discoverCharacteristics(characteristicUUIDs)
        return characteristics.map { $0 as [CharacteristicType] }
    }
}

public extension ObservableType where Element == [ServiceType] {
    /// Returns the service that has the UUID passed as parameter.
    /// If the service does not exist in the list emitted by the observable a "serviceNotFound" error will be thrown.
    ///
    /// - Parameter serviceUUID: The UUID of the service that is required.
    /// - Returns: Observable that emits the required service.
    func requiredService(_ serviceUUID: CBUUID) -> Observable<ServiceType> {
        return requiredService(filter: [serviceUUID])
    }

    /// Returns the service that has the UUID among the list passed as parameter.
    /// If the service does not exist in the list emitted by the observable a "serviceNotFound" error will be thrown.
    ///
    /// - Parameter serviceUUIDs: A list of possible UUIDs of the service that is required.
    /// - Returns: Observable that emits the required service.
    func requiredService(filter serviceUUIDs: [CBUUID]) -> Observable<ServiceType> {
        return map { services -> ServiceType in
            guard let service = services.first(where: { serviceUUIDs.contains($0.uuid) })
            else { throw PeripheralError.serviceNotFound(serviceUUIDs) }

            return service
        }
    }
}

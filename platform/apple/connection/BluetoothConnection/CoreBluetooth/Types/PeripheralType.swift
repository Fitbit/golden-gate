//
//  PeripheralType.swift
//  BluetoothConnection
//
//  Created by Emanuel Jarnea on 18/11/2020.
//  Copyright © 2020 Fitbit. All rights reserved.
//

import CoreBluetooth
import Foundation
import RxBluetoothKit
import RxSwift

public enum PeripheralError: Error {
    case serviceNotFound([CBUUID]) // Holds the list of expected UUIDs for a service
    case servicesChanged
    case missingCharacteristic(CBUUID)
    case unexpectedCharacteristicProperties(CBUUID)
}

/// Peripheral is a ReactiveX wrapper over Core Bluetooth functions allowing to talk to
/// a peripheral like discovering characteristics, services and all of the read/write calls.
public protocol PeripheralType: AnyObject {
    /// Current name of peripheral instance.
    var name: String? { get }

    /// Unique identifier of peripheral instance. Assigned once peripheral is discovered by the system.
    var identifier: UUID { get }

    /// A list of services that have been discovered.
    func services() -> [ServiceType]?

    /// Value indicating if peripheral is currently in connected state.
    var isConnected: Bool { get }

    /// Observable that emits true when ANCS is authorized and false otherwise.
    var isANCSAuthorized: Observable<Bool> { get }

    /// Establishes connection with a given peripheral.
    func establishConnection(options: [String: Any]?) -> Observable<PeripheralType>

    /// Triggers discover of specified services of peripheral.
    func discoverServices(_ serviceUUIDs: [CBUUID]?) -> Single<[ServiceType]>

    /// Function that allows to observe incoming service modifications for the peripheral instance.
    func observeServicesModification() -> Observable<(PeripheralType, [ServiceType])>
    
    /// The maximum amount of data, in bytes, that can be sent to a characteristic in a single write.
    func maximumWriteValueLength(for type: CBCharacteristicWriteType) -> Int

    /// Function that triggers write of data to characteristic.
    func writeValue(
        _ data: Data,
        for characteristic: CharacteristicType,
        type: CBCharacteristicWriteType,
        canSendWriteWithoutResponseCheckEnabled: Bool
    ) -> Single<CharacteristicType>

    /// Function that triggers read of peripheral RSSI value.
    func readRSSI() -> Single<(PeripheralType, Int)>
}

extension Peripheral: PeripheralType {
    public func services() -> [ServiceType]? { services }

    public var isANCSAuthorized: Observable<Bool> {
#if os(iOS)
        if #available(iOS 13.3, *) {
            return manager
                .observeANCSAuthorized(for: self)
                .startWith(peripheral.ancsAuthorized)
        } else {
            return .just(true)
        }
#else
        return .just(true)
#endif
    }

    public func establishConnection(options: [String : Any]? = nil) -> Observable<PeripheralType> {
        let connection: Observable<Peripheral> = establishConnection(options: options)
        return connection.map { $0 as PeripheralType }
    }

    public func discoverServices(_ serviceUUIDs: [CBUUID]?) -> Single<[ServiceType]> {
        let services: Single<[Service]> = discoverServices(serviceUUIDs)
        return services.map { $0 as [ServiceType] }
    }

    public func observeServicesModification() -> Observable<(PeripheralType, [ServiceType])> {
        let observation: Observable<(Peripheral, [Service])> = observeServicesModification()
        return observation.map { $0 as (PeripheralType, [ServiceType]) }
    }

    public func writeValue(
        _ data: Data,
        for characteristic: CharacteristicType,
        type: CBCharacteristicWriteType,
        canSendWriteWithoutResponseCheckEnabled: Bool
    ) -> Single<CharacteristicType> {
        let write: Single<Characteristic> = writeValue(
            data,
            for: characteristic as! Characteristic,
            type: type,
            canSendWriteWithoutResponseCheckEnabled: canSendWriteWithoutResponseCheckEnabled
        )
        return write.map { $0 as CharacteristicType }
    }

    public func readRSSI() -> Single<(PeripheralType, Int)> {
        let rssi: Single<(Peripheral, Int)> = readRSSI()
        return rssi.map { $0 as (PeripheralType, Int) }
    }
}

extension Peripheral: CustomStringConvertible {
    public var description: String {
        return "\(type(of: self)) \(peripheral.name ??? "nil") (\(peripheral.identifier))"
    }
}

public extension ConnectionStatus where Connection == Peripheral {
    var peripheral: Peripheral? { connection }
}

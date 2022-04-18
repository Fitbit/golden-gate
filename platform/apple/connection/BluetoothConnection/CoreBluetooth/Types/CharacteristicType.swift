//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  CharacteristicType.swift
//  BluetoothConnection
//
//  Created by Emanuel Jarnea on 18/11/2020.
//

import CoreBluetooth
import Foundation
import RxBluetoothKit
import RxSwift

/// CharacteristicType is a ReactiveX wrapper over CoreBluetooth's CBCharacteristic
/// https://developer.apple.com/documentation/corebluetooth/cbcharacteristic
public protocol CharacteristicType: AnyObject {
    /// The Bluetooth UUID of the characteristic instance.
    var uuid: CBUUID { get }

    /// Service which contains this characteristic
    func service() -> ServiceType

    /// Properties of characteristic
    var properties: CBCharacteristicProperties { get }

    /// Current value of characteristic. If value is not present - it's `nil`.
    var value: Data? { get }

    /// Accesses the characteristic which is supposed to be secure.
    func secureAccess() -> Completable

    /// Function that triggers read of current value of the `Characteristic` instance.
    func readValue() -> Single<CharacteristicType>

    /// Setup characteristic notification in order to receive callbacks when given characteristic has been changed.
    func observeValueUpdateAndSetNotification() -> Observable<CharacteristicType>

    /// Function that triggers write of data to characteristic.
    func writeValue(_ data: Data, type: CBCharacteristicWriteType) -> Single<CharacteristicType>
}

extension Characteristic: CharacteristicType {
    public func service() -> ServiceType { service }

    public func readValue() -> Single<CharacteristicType> {
        let value: Single<Characteristic> = readValue()
        return value.map { $0 as CharacteristicType }
    }

    public func observeValueUpdateAndSetNotification() -> Observable<CharacteristicType> {
        let value: Observable<Characteristic> = observeValueUpdateAndSetNotification()
        return value.map { $0 as CharacteristicType }
    }

    public func writeValue(_ data: Data, type: CBCharacteristicWriteType) -> Single<CharacteristicType> {
        let write: Single<Characteristic> = writeValue(data, type: type)
        return write.map { $0 as CharacteristicType }
    }
}

extension CharacteristicType {
    /// Returns an observable that emits data as it is received from the characteristic.
    /// The initial value is read, if the characteristics permits reads.
    public func readAndObserveValue() -> Observable<Data?> {
        let readValueIfSupported: Observable<CharacteristicType>

        if properties.contains(.read) {
            readValueIfSupported = readValue().asObservable()
        } else {
            readValueIfSupported = .empty()
        }

        return Observable.merge(
            observeValueUpdateAndSetNotification(),
            readValueIfSupported
        )
        .map { $0.value }
    }

    public func secureAccess() -> Completable {
        readValue().asCompletable()
            .logError("Access Secure Characteristic error", .bluetooth, .error)
            .catch { error in
                switch error {
                case BluetoothError.characteristicReadFailed(_, let bluetoothError as CBATTError) where bluetoothError.code == CBATTError.insufficientEncryption:
                    throw CharacteristicSecureAccessError.insufficientEncryption
                case BluetoothError.characteristicReadFailed(_, let bluetoothError as CBATTError) where bluetoothError.code == CBATTError.insufficientAuthentication:
                    throw CharacteristicSecureAccessError.insufficientAuthentication
                default:
                    throw CharacteristicSecureAccessError.other(underlyingAccessError: error)
                }
            }
    }
}

public enum CharacteristicSecureAccessError: Error {
    case insufficientEncryption
    case insufficientAuthentication
    case other(underlyingAccessError: Error)
}

extension CharacteristicSecureAccessError: CustomStringConvertible {
    public var description: String {
        switch self {
        case .insufficientEncryption:
            return "Insufficient Encryption"
        case .insufficientAuthentication:
            return "Insufficient Authentication"
        case .other(let underlyingAccessError):
            return "Other Access Error: \(underlyingAccessError)"
        }
    }
}

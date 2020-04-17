//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  Hub.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 11/9/17.
//

import CoreBluetooth
import Foundation
import GoldenGateXP
import Rxbit
import RxBluetoothKit
import RxCocoa
import RxSwift

public enum HubError: Error {
    case illegalGoldenGateStatus
    case illegalConnectionConfiguration
    case illegalConnectionStatus
    case illegalBatteryServiceLevel
    case illegalEphemeralCharacteristicPointer(CBUUID)
    case networkLinkStatusTimeout
    case insufficientEncryption
    case insufficientAuthentication
    case other(underlyingError: Error)
}

extension HubError: CustomStringConvertible {
    public var description: String {
        switch self {
        case .illegalGoldenGateStatus:
            return "Illegal GoldenGate Status"
        case .illegalConnectionConfiguration:
            return "Illegal Connection Configuration"
        case .illegalConnectionStatus:
            return "Illegal Connection Status"
        case .illegalBatteryServiceLevel:
            return "Illegal Battery Service Level"
        case .illegalEphemeralCharacteristicPointer(let uuid):
            return "Illegal Ephemeral Characteristic Pointer: \(uuid)"
        case .networkLinkStatusTimeout:
            return "Network Link Status Timeout"
        case .insufficientEncryption:
            return "Insufficient Encryption"
        case .insufficientAuthentication:
            return "Insufficient Authentication"
        case .other(let underlyingError):
            return "Other: \(underlyingError)"
        }
    }
}

let networkLinkEstablishmentTimeout: RxTimeInterval = .seconds(15)

/// A GoldenGate Hub.
public class Hub: Connector {
    static private let delayForServiceChangeIndication: DispatchTimeInterval = .seconds(5)

    public typealias Descriptor = BluetoothPeerDescriptor
    public typealias ConnectionType = GoldenGate.Connection

    internal let runLoop: RunLoop
    internal let configuration: BluetoothConfiguration
    private let centralManager: CentralManager
    private let peripheralManager: PeripheralManager
    private let publishedServices: Completable
    private let disposeBag = DisposeBag()

    public let linkConfigurationService: LinkConfigurationService

    public init(
        runLoop: RunLoop,
        centralManager: CentralManager,
        peripheralManager: PeripheralManager,
        configuration: BluetoothConfiguration
    ) {
        self.runLoop = runLoop
        self.centralManager = centralManager
        self.peripheralManager = peripheralManager
        self.configuration = configuration
        self.linkConfigurationService = LinkConfigurationService(configuration: configuration, peripheralManager: peripheralManager)

        let publishedLinkConfigurationService = peripheralManager
            .publish(service: linkConfigurationService)
            .asObservable()

        // Create a ref-counted intent to publish services
        self.publishedServices = publishedLinkConfigurationService
            .publish()
            .refCount()
            .ignoreElements()
    }

    // swiftlint:disable:next function_body_length
    public func establishConnection(with descriptor: BluetoothPeerDescriptor) -> Observable<ConnectionType> {
        // Establish peripheral connection and start discovering the services and characteristics.
        //
        // Note: Initially we used `replay(1)` for this instead of `multicast(ReplaySubject.create(bufferSize: 1))`.
        // However, replay internally uses `multicast(makeSubject:)` that will clear the subject in case source
        // sequence produces terminal event. Because of that, any error event would only occur in the `resourceFactory`,
        // but not also in `remoteConnectionConfiguration` for example.
        let peripheralConnectionStatus = centralManager
            .peripheralConnectionStatus(for: descriptor.identifier)
            .multicast(ReplaySubject.create(bufferSize: 1))

        let resourceFactory = { [publishedServices] () -> CompositeDisposable in
            // We need a concrete type that conforms to `Disposable` and
            // can't return `Disposable` directly, because Swift won't allow it.
            // See: https://github.com/ReactiveX/RxSwift/issues/925
            let disposable = CompositeDisposable()
            disposable += publishedServices.subscribe()
            disposable += peripheralConnectionStatus.connect()
            return disposable
        }

        return Observable.using(resourceFactory) { [configuration] (_) -> Observable<ConnectionType> in
            // RxBluetoothkit requires us to discover all services we may need in one
            // shot because of [https://github.com/Polidea/RxBluetoothKit/issues/309].
            let services = peripheralConnectionStatus
                .map { $0.peripheral }
                .filterNil()
                .take(1)
                .flatMap { [configuration] peripheral -> Observable<[Service]> in
                    return peripheral.validServices([
                        configuration.linkStatusService.serviceUUID,
                        configuration.gattlinkService.serviceUUID,
                        configuration.batteryLevelService.serviceUUID,
                        configuration.confirmationService.serviceUUID
                    ], blacklistedServiceUUIDMatcher: configuration.legacyServiceUUIDMatcher)
                }
                .share(replay: 1)

            let networkLinkStatus = self.networkLinkStatus(
                    for: peripheralConnectionStatus,
                    discoveredServices: services
                )
                .do(onError: { [weak self] error in
                    LogBindingsError("\(self ??? "Hub").Connection Status Error: \(error)")
                })
                .share(replay: 1)

            let linkCharacteristics = services
                .requiredService(configuration.linkStatusService.serviceUUID)
                .flatMapLatest { [configuration] service -> Observable<CharacteristicCollection> in
                    return service.discoverPartialCharacteristics([
                        configuration.linkStatusService.currentConnectionConfigurationUUID,
                        configuration.linkStatusService.currentConnectionStatusUUID,
                        configuration.linkStatusService.bondSecureUUID
                    ])
                }
                .share(replay: 1)

            let remoteConnectionConfiguration = linkCharacteristics
                .map { [configuration] in
                    try $0.required(configuration.linkStatusService.currentConnectionConfigurationUUID)
                }
                .flatMapLatest(self.remoteConnectionConfiguration(from:))
                .do(onNext: { [weak self] configuration in
                    LogBindingsInfo("\(self ??? "Hub"): Connection configuration updated with mode \(configuration.mode)")
                })
                .do(onError: { [weak self] error in
                    LogBindingsError("\(self ??? "Hub").Remote Link Configuration: \(error)")
                })
                // Ignore all errors for now until node exposes this characteristic
                .catchError { _ in .empty() }
                .share(replay: 1)

            let remoteConnectionStatus = linkCharacteristics
                .map { [configuration] in
                    try $0.required(configuration.linkStatusService.currentConnectionStatusUUID)
                }
                .flatMapLatest(self.remoteConnectionStatus(from:))
                .do(onError: { [weak self] error in
                    LogBindingsError("\(self ??? "Hub").Remote Link Status: \(error)")
                })
                // Ignore all errors for now until node exposes this characteristic
                .catchError { _ in .empty() }
                .share(replay: 1)

            let accessBondSecureCharacteristic = linkCharacteristics
                .map { [configuration] in
                    try $0.required(configuration.linkStatusService.bondSecureUUID)
                }
                .take(1)
                .flatMapLatest { $0.readValue() }
                .ignoreElements()
                .do(onError: { [weak self] error in
                    LogBindingsError("\(self ??? "Hub").Access Secure Characteristic: \(error)")
                })
                .catchError { error in
                    switch error {
                    case BluetoothError.characteristicReadFailed(_, let bluetoothError as CBATTError) where bluetoothError.code == CBATTError.insufficientEncryption:
                        throw HubError.insufficientEncryption
                    case BluetoothError.characteristicReadFailed(_, let bluetoothError as CBATTError) where bluetoothError.code == CBATTError.insufficientAuthentication:
                        throw HubError.insufficientAuthentication
                    default:
                        throw HubError.other(underlyingError: error)
                    }
                }

            let remoteBatteryLevel = services
                .requiredService(configuration.batteryLevelService.serviceUUID)
                .flatMapLatest { service -> Observable<CharacteristicCollection> in
                    return service.discoverPartialCharacteristics([
                        configuration.batteryLevelService.batteryLevelUUID
                    ])
                }
                .map { [configuration] in
                    try $0.required(configuration.batteryLevelService.batteryLevelUUID)
                }
                .flatMapLatest(self.remoteBatteryLevel(from:))
                .do(onError: { [weak self] error in
                    LogBindingsError("\(self ??? "Hub").Remote Battery Level: \(error)")
                })
                // Ignore all errors for now until node exposes this characteristic
                .catchError { _ in .empty() }

            let ancsAuthorized = peripheralConnectionStatus
                .map { $0.peripheral }
                .filterNil()
                .take(1)
                .flatMap { peripheral -> Observable<Bool> in
                    #if os(iOS)
                        //if #available(iOS 13.3, *) {
                        //    return self.centralManager.manager.observeANCSAuthorized(for: peripheral)
                        //        .startWith(peripheral.peripheral.ancsAuthorized)
                        //} else {
                            return .just(true)
                        //}
                    #else
                        return .just(true)
                    #endif
                }

            // Create the connection object and pass all the observables needed.
            let connection = Connection(
                descriptor: descriptor,
                peripheralConnection: peripheralConnectionStatus,
                networkLinkStatus: networkLinkStatus,
                remoteConnectionConfiguration: remoteConnectionConfiguration,
                remoteConnectionStatus: remoteConnectionStatus,
                accessBondSecureCharacteristic: accessBondSecureCharacteristic,
                remoteBatteryLevel: remoteBatteryLevel,
                ancsAuthorized: ancsAuthorized
            )

            // Error out if we are connected to a node but we couldn't find the Gattlink service within
            // within `networkLinkEstablishmentTimeout`.
            let networkLinkStatusTimeout = Observable.combineLatest(peripheralConnectionStatus, networkLinkStatus)
                .map { tuple -> Bool in
                    if case (.connected, .negotiating) = tuple {
                        return true
                    } else {
                        return false
                    }
                }
                .distinctUntilChanged()
                .debounce(networkLinkEstablishmentTimeout, scheduler: SerialDispatchQueueScheduler(qos: .default))
                .flatMapLatest { isMissing -> Completable in
                    if isMissing {
                        return Completable.error(HubError.networkLinkStatusTimeout)
                    } else {
                        return Completable.never()
                    }
                }
                .logWarning("Failed to establish Network Link: ", .bindings, .error)

            let writeEphemeralCharacteristic = self.writeEphemeralCharacteristic(services)
                // Ignore all errors for now until node exposes this characteristic
                .catchError { _ in .empty() }

            return Observable.just(connection)
                // Keep the observable connection alive until an error occurs
                .concat(Observable<ConnectionType>.never())
                .inheritError(from: remoteConnectionConfiguration)
                .inheritError(from: remoteConnectionStatus)
                .inheritError(from: networkLinkStatus)
                .inheritError(from: networkLinkStatusTimeout)
                // By inheriting from writeEphemeralCharacteristic, it has the side
                // effect of telling the node we've discovered new services without needing
                // to send iOS a service change indication.
                .inheritError(from: writeEphemeralCharacteristic)
                .do(onNext: { [weak self] connection in
                    LogBindingsInfo("\(self ??? "Hub").establishConnection: \(connection)")
                })
                .catchError { error in
                    LogBindingsError("\(self ??? "Hub").establishConnection: Error occured \(error)")

                    switch error {
                    case BluetoothError.characteristicReadFailed(_, CBATTError.invalidHandle?):
                        // When this error is received, the connection must be kept alive a period of
                        // time in order to wait for a possible service indication sent by the peripheral.
                        // Delaying sending the error...
                        LogBluetoothError("Invalid characteristic handler.")
                        return Observable<ConnectionType>.error(error)
                            .delaySubscription(Hub.delayForServiceChangeIndication, scheduler: SerialDispatchQueueScheduler(qos: .default))
                    default: throw error
                    }
                }
        }
    }
}

extension Hub {
    func writeEphemeralCharacteristic(_ services: Observable<[Service]>) -> Completable {
        let confirmationCharacteristics = services
            .requiredService(configuration.confirmationService.serviceUUID)
            .flatMapLatest { service -> Observable<[Characteristic]> in
                // perform a wildcard search for characteristics to detect the random one the
                // tracker created when its database changed (see Ephemeral Characteristic Spec)
                return service.discoverCharacteristics(nil).asObservable()
            }
            .map { CharacteristicCollection(characteristics: $0) }
            .do(onError: { [weak self] error in
                LogBluetoothError("\(self ??? "Hub").Confirmation Service: \(error)")
            })
            .share(replay: 1)

        // Get the ephemeral characteristic UUID using the ephemeral
        // characteristic pointer.
        let ephemeralCharacteristicPointer = confirmationCharacteristics
            .map { [configuration] in
                try $0.required(configuration.confirmationService.ephemeralCharacteristicPointerUUID)
            }
            .flatMapLatest {
                $0.readValue()
            }

        return Observable.combineLatest(ephemeralCharacteristicPointer, confirmationCharacteristics)
            .map { [configuration] characteristic, confirmationChracteristics -> Characteristic? in
                guard let ephemeralPointerData = characteristic.value else {
                    throw HubError.illegalEphemeralCharacteristicPointer(
                        configuration.confirmationService.ephemeralCharacteristicPointerUUID
                    )
                }
                let ephemeralCharacteristicUUID = CBUUID(data: ephemeralPointerData)
                LogBluetoothInfo("Ephemeral characteristic UUID: \(ephemeralCharacteristicUUID)")

                // return nil if the characteristic was not discovered
                // to indicate that we should wait for a service change indication from tracker
                return confirmationChracteristics.optional(ephemeralCharacteristicUUID)
            }
            .filterNil()
            .take(1)
            .flatMap { ephemeralCharacteristic -> Single<Characteristic> in
                LogBluetoothInfo("Writing 0x00 to ephemeral characteristic")

                return ephemeralCharacteristic.writeValue(Data([0x00]), type: .withResponse)
            }
            .ignoreElements()
    }
}

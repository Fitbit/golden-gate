//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  Node.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 11/9/17.
//

import CoreBluetooth
import Foundation
import Rxbit
import RxBluetoothKit
import RxCocoa
import RxSwift

enum NodeError: Error {
    case unexpectedCharacteristicProperties
    case illegalConnectionConfiguration
    case illegalConnectionMode
}

/// A GoldenGate Node (i.e. it connects to a GoldenGate Hub).
public class Node: Connector {
    public typealias Descriptor = BluetoothPeerDescriptor
    public typealias ConnectionType = GoldenGate.Connection

    internal let runLoop: RunLoop
    internal let configuration: BluetoothConfiguration
    private let centralManager: CentralManager
    private let peripheralManager: PeripheralManager
    private let gattlinkService: GattlinkService

    public init(
        runLoop: RunLoop,
        centralManager: CentralManager,
        peripheralManager: PeripheralManager,
        gattlinkService: GattlinkService,
        configuration: BluetoothConfiguration
    ) {
        self.runLoop = runLoop
        self.centralManager = centralManager
        self.peripheralManager = peripheralManager
        self.gattlinkService = gattlinkService
        self.configuration = configuration
    }

    // swiftlint:disable:next function_body_length
    public func establishConnection(with descriptor: BluetoothPeerDescriptor) -> Observable<ConnectionType> {
        // Establish peripheral connection and start discovering the services and characteristics.
        let peripheralConnectionStatus = centralManager
            .peripheralConnectionStatus(for: descriptor.identifier)
            .replay(1)

        let resourceFactory = { () -> CompositeDisposable in
            // We need a concrete type that conforms to `Disposable` and
            // can't return `Disposable` directly, because Swift won't allow it.
            // See: https://github.com/ReactiveX/RxSwift/issues/925
            let disposable = CompositeDisposable()
            disposable += peripheralConnectionStatus.connect()
            return disposable
        }

        return Observable.using(resourceFactory, observableFactory: { [gattlinkService, peripheralManager, configuration] (_) -> Observable<ConnectionType> in
            // Listen for subscriptions to the Gattlink service.
            let networkLinkStatus = self.networkLinkStatus(
                    for: descriptor.identifier,
                    gattlinkService: gattlinkService,
                    peripheralManager: peripheralManager
                )
                .do(onError: { [weak self] error in
                    LogBindingsError("\(self ??? "Node").Connection Status Error: \(error)")
                })
                .share(replay: 1)

            // RxBluetoothkit requires us to discover all services we may need in one
            // shot because of [Github Issue].
            let services = peripheralConnectionStatus
                .map { $0.peripheral }
                .filterNil()
                .take(1)
                .flatMap { [configuration] peripheral -> Observable<[Service]> in
                    return peripheral.validServices([
                        configuration.linkConfigurationService.serviceUUID
                    ])
                }

            let linkConfigurationCharacteristics = services
                .requiredService(configuration.linkConfigurationService.serviceUUID)
                .flatMapLatest { service -> Observable<CharacteristicCollection> in
                    return service.discoverPartialCharacteristics([
                        configuration.linkConfigurationService.preferredConnectionConfigurationUUID,
                        configuration.linkConfigurationService.preferredConnectionModeUUID
                    ])
                }
                .share(replay: 1)

            let remotePreferredConnectionConfiguration = linkConfigurationCharacteristics
                .map { [configuration] in
                    try $0.required(configuration.linkConfigurationService.preferredConnectionConfigurationUUID)
                }
                .flatMapLatest(self.remotePreferredConnectionConfiguration(from:))
                .logError("Remote Preferred Connection Configuration", .bindings, .error)

            let remotePreferredConnectionMode = linkConfigurationCharacteristics
                .map { [configuration] in
                    try $0.required(configuration.linkConfigurationService.preferredConnectionModeUUID)
                }
                .flatMapLatest(self.remotePreferredConnectionMode(from:))
                .logError("Remote Preferred Connection Mode", .bindings, .error)

            // Create the connection object and pass all the observables needed.
            let connection = Connection(
                networkLinkStatus: networkLinkStatus,
                descriptor: descriptor,
                peripheralConnectionStatus: peripheralConnectionStatus,
                remotePreferredConnectionConfiguration: remotePreferredConnectionConfiguration,
                remotePreferredConnectionMode: remotePreferredConnectionMode
            )

            return Observable.just(connection)
                .concat(Observable<ConnectionType>.never())
                .inheritError(from: networkLinkStatus)
        })
    }
}

private extension Node {
    func remotePreferredConnectionConfiguration(
        from characteristic: Characteristic
    ) -> Observable<LinkConfigurationService.PreferredConnectionConfiguration> {
        return Observable.merge(
                characteristic.observeValueUpdateAndSetNotification(),
                characteristic.readValue().asObservable()
            )
            .map {
                guard
                    let value = $0.value,
                    let configuration = LinkConfigurationService.PreferredConnectionConfiguration(rawValue: value)
                else {
                    throw NodeError.illegalConnectionConfiguration
                }

                return configuration
            }
    }

    func remotePreferredConnectionMode(
        from characteristic: Characteristic
    ) -> Observable<LinkConfigurationService.PreferredConnectionMode> {
        return Observable.merge(
                characteristic.observeValueUpdateAndSetNotification(),
                characteristic.readValue().asObservable()
            )
            .map {
                guard
                    let value = $0.value,
                    let configuration = LinkConfigurationService.PreferredConnectionMode(rawValue: value)
                else {
                    throw NodeError.illegalConnectionMode
                }

                return configuration
            }
    }
}

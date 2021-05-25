//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  Node.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 11/9/17.
//

import BluetoothConnection
import RxBluetoothKit
import RxSwift

enum NodeError: Error {
    case illegalConnectionConfiguration
    case illegalConnectionMode
}

/// A GoldenGate Node (i.e. it resolves connections to a GoldenGate Hub).
public class Node: ConnectionResolver {
    private let bluetoothScheduler: SchedulerType
    internal let protocolScheduler: ImmediateSchedulerType
    internal let configuration: BluetoothConfiguration
    private let peripheralManager: BluetoothConnection.PeripheralManager
    private let linkService: LinkService
    internal let dataSinkBufferSize: UInt

    public init(
        bluetoothScheduler: SchedulerType,
        protocolScheduler: ImmediateSchedulerType,
        peripheralManager: BluetoothConnection.PeripheralManager,
        linkService: LinkService,
        configuration: BluetoothConfiguration,
        dataSinkBufferSize: UInt
    ) {
        self.bluetoothScheduler = bluetoothScheduler
        self.protocolScheduler = protocolScheduler
        self.peripheralManager = peripheralManager
        self.linkService = linkService
        self.configuration = configuration
        self.dataSinkBufferSize = dataSinkBufferSize
    }

    public func resolveConnection(
        services: [BluetoothConnector.DiscoveredService],
        descriptor: PeerDescriptor
    ) -> Observable<NodeConnection> {
        ensureExpectedCharacteristics(inServices: services)
            .catchError { error in
                switch error {
                case PeripheralError.serviceNotFound, PeripheralError.missingCharacteristic:
                    throw ConnectionResolverError.couldNotResolveConnection
                default:
                    throw error
                }
            }
            .do(onSuccess: { _ in LogInfo("Node: Ensured expected characteristics are available", domain: .bluetooth) })
            .asObservable()
            .flatMap { characteristics in
                self.createConnection(characteristics: characteristics, descriptor: descriptor)
            }
    }

    private func createConnection(
        characteristics: ExpectedCharacteristics,
        descriptor: PeerDescriptor
    ) -> Observable<NodeConnection> {
        let networkLink = self.networkLink(
            for: descriptor.identifier,
            linkService: linkService,
            peripheralManager: peripheralManager
        )
        .share(replay: 1)

        let remotePreferredConnectionConfiguration = self.remotePreferredConnectionConfiguration(
                from: characteristics.linkConfiguration.preferredConnectionConfiguration
            )
            .logError("Node: Remote Preferred Connection Configuration error", .bluetooth, .error)

        let remotePreferredConnectionMode = self.remotePreferredConnectionMode(from: characteristics.linkConfiguration.preferredConnectionMode)
            .logError("Node: Remote Preferred Connection Mode error", .bluetooth, .error)

        let connection = NodeConnection(
            descriptor: descriptor,
            networkLink: networkLink,
            remotePreferredConnectionConfiguration: remotePreferredConnectionConfiguration,
            remotePreferredConnectionMode: remotePreferredConnectionMode
        )

        return Observable.just(connection)
            .concat(Observable<ConnectionType>.never())
            .inheritError(from: networkLink)
    }
}

// swiftlint:disable nesting

private extension Node {
    /// A collection of all the required and optional characteristics needed to resolve a node connection.
    struct ExpectedCharacteristics {
        struct LinkConfiguration {
            let preferredConnectionConfiguration: CharacteristicType
            let preferredConnectionMode: CharacteristicType
        }

        let linkConfiguration: LinkConfiguration
    }

    /// Iterates the discovered services and attempts to create a collection of all the required and optional characteristics
    /// needed to resolve a node connection.
    func ensureExpectedCharacteristics(inServices services: [BluetoothConnector.DiscoveredService]) -> Single<ExpectedCharacteristics> {
        Single.deferred {
            // Link configuration service

            let linkConfiguration: Single<ExpectedCharacteristics.LinkConfiguration>

            do {
                let serviceConfiguration = self.configuration.linkConfigurationService

                let allCharacteristics = try services.required(serviceConfiguration.serviceUUID).characteristics

                linkConfiguration = allCharacteristics.map { characteristics in
                    ExpectedCharacteristics.LinkConfiguration(
                        preferredConnectionConfiguration: try characteristics.required(serviceConfiguration.preferredConnectionConfigurationUUID),
                        preferredConnectionMode: try characteristics.required(serviceConfiguration.preferredConnectionModeUUID)
                    )
                }
            }

            return linkConfiguration.map { ExpectedCharacteristics(linkConfiguration: $0) }
        }
    }
}

private extension Node {
    func remotePreferredConnectionConfiguration(
        from characteristic: CharacteristicType
    ) -> Observable<LinkConfigurationService.PreferredConnectionConfiguration> {
        return characteristic.readAndObserveValue()
            .map {
                guard
                    let value = $0,
                    let configuration = LinkConfigurationService.PreferredConnectionConfiguration(rawValue: value)
                else {
                    throw NodeError.illegalConnectionConfiguration
                }

                return configuration
            }
    }

    func remotePreferredConnectionMode(
        from characteristic: CharacteristicType
    ) -> Observable<LinkConfigurationService.PreferredConnectionMode> {
        return characteristic.readAndObserveValue()
            .map {
                guard
                    let value = $0,
                    let configuration = LinkConfigurationService.PreferredConnectionMode(rawValue: value)
                else {
                    throw NodeError.illegalConnectionMode
                }

                return configuration
            }
    }
}

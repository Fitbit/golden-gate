//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  Hub.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 11/9/17.
//

import BluetoothConnection
import BluetoothConnectionObjC
import CoreBluetooth
import Foundation
import RxBluetoothKit
import RxSwift

// swiftlint:disable nesting
// swiftlint:disable identifier_name

public enum HubError: Error {
    case illegalConnectionConfiguration
    case illegalConnectionStatus
    case illegalEphemeralCharacteristicPointer(Data?)
}

extension HubError: CustomStringConvertible {
    public var description: String {
        switch self {
        case .illegalConnectionConfiguration:
            return "Illegal Connection Configuration"
        case .illegalConnectionStatus:
            return "Illegal Connection Status"
        case .illegalEphemeralCharacteristicPointer(let data):
            return "Illegal Ephemeral Characteristic Pointer: \(String(describing: data as NSData?))"
        }
    }
}

/// A GoldenGate Hub.
public class Hub: ConnectionResolver {
    static private let delayForServiceChangeIndication: DispatchTimeInterval = .seconds(5)

    private let bluetoothScheduler: SchedulerType
    internal let protocolScheduler: ImmediateSchedulerType
    internal let configuration: BluetoothConfiguration

    public init(
        bluetoothScheduler: SchedulerType,
        protocolScheduler: ImmediateSchedulerType,
        configuration: BluetoothConfiguration
    ) {
        self.bluetoothScheduler = bluetoothScheduler
        self.protocolScheduler = protocolScheduler
        self.configuration = configuration
    }

    public func resolveConnection(
        services: [BluetoothConnector.DiscoveredService],
        descriptor: PeerDescriptor
    ) -> Observable<HubConnection> {
        ensureExpectedCharacteristics(inServices: services)
            .catchError { error in
                switch error {
                case PeripheralError.serviceNotFound, PeripheralError.missingCharacteristic:
                    throw ConnectionResolverError.couldNotResolveConnection
                default:
                    throw error
                }
            }
            .do(onSuccess: { _ in LogInfo("Hub: Ensured expected characteristics are available", domain: .bluetooth) })
            .asObservable()
            .flatMap { characteristics in
                self.createConnection(characteristics: characteristics, descriptor: descriptor)
            }
    }
    // swiftlint:disable:next function_body_length
    private func createConnection(
        characteristics: ExpectedCharacteristics,
        descriptor: PeerDescriptor
    ) -> Observable<HubConnection> {
        let networkLink: Observable<NetworkLink>
        do {
            networkLink = .just(
                try self.networkLink(
                    rxCharacteristic: characteristics.link.rx,
                    txCharacteristic: characteristics.link.tx
                )
            )
        } catch {
            networkLink = .error(error)
        }

        let remoteConnectionConfiguration = { () -> Observable<LinkStatusService.ConnectionConfiguration> in
            guard let characteristic = characteristics.linkStatus?.currentConnectionConfiguration else {
                return .empty()
            }

            return self.remoteConnectionConfiguration(from: characteristic)
                .do(onNext: { configuration in
                    LogBluetoothInfo("Hub: Remote connection configuration updated with mode \(configuration.mode)")
                })
                .logError("Hub: Remote connection configuration error:", .bluetooth, .error)
                .share(replay: 1)
        }()

        let remoteConnectionStatus = { () -> Observable<LinkStatusService.ConnectionStatus> in
            guard let characteristic = characteristics.linkStatus?.currentConnectionStatus else {
                return .empty()
            }

            return self.remoteConnectionStatus(from: characteristic)
                .logError("Hub: Remote connection status error:", .bluetooth, .error)
                .share(replay: 1)
        }()

        let accessBondSecureCharacteristic = { () -> Completable in
            guard let characteristic = characteristics.linkStatus?.bondSecure else {
                return .empty()
            }

            return characteristic.secureAccess()
        }()

        let writeEphemeralCharacteristic = { () -> Completable in
            guard let confirmation = characteristics.confirmation else {
                return .empty()
            }
            return self.writeEphemeralCharacteristic(
                ephemeralCharacteristicPointer: confirmation.ephemeralCharacteristicPointer,
                confirmationCharacteristics: confirmation.all
            )
            .logError("Hub: Ephemeral characteristic error:", .bluetooth, .error)
            .catchError { error in
                switch error {
                case HubError.illegalEphemeralCharacteristicPointer:
                    // Do not fail when the characteristic pointer is invalid.
                    return .empty()
                default:
                    throw error
                }
            }
        }()

        let connection = HubConnection(
            descriptor: descriptor,
            ancsAuthorized: characteristics.link.rx.service().peripheral().isANCSAuthorized,
            networkLink: networkLink,
            remoteConnectionConfiguration: remoteConnectionConfiguration,
            remoteConnectionStatus: remoteConnectionStatus,
            accessBondSecureCharacteristic: accessBondSecureCharacteristic
        )

        return Observable.just(connection)
            .concat(Observable.never())
            // Keep the observable connection alive until an error occurs
            .inheritError(from: remoteConnectionConfiguration)
            .inheritError(from: remoteConnectionStatus)
            .inheritError(from: networkLink)
            // By inheriting from writeEphemeralCharacteristic, it has the side
            // effect of telling the node we've discovered new services without needing
            // to send iOS a service change indication.
            .inheritError(from: writeEphemeralCharacteristic)
            .logInfo("Hub: resolveConnection", .bluetooth, .next)
            .catchError { error in
                LogBluetoothError("Hub: Error occured in resolveConnection: \(error)")

                switch error {
                case BluetoothError.characteristicReadFailed(_, CBATTError.invalidHandle?):
                    // When this error is received, the connection must be kept alive a period of
                    // time in order to wait for a possible service indication sent by the peripheral.
                    // Delaying sending the error...
                    LogBluetoothError("Hub: Invalid characteristic handler in resolveConnection.")
                    return Observable.error(error)
                        .delaySubscription(Self.delayForServiceChangeIndication, scheduler: self.bluetoothScheduler)
                default: throw error
                }
            }
            .delayUnknownDisconnectionError(scheduler: self.bluetoothScheduler)
    }
}

private extension Hub {
    /// A collection of all the required and optional characteristics needed to resolve a hub connection.
    struct ExpectedCharacteristics {
        struct Link {
            let rx: CharacteristicType
            let tx: CharacteristicType
        }

        struct LinkStatus {
            let currentConnectionConfiguration: CharacteristicType
            let currentConnectionStatus: CharacteristicType
            let bondSecure: CharacteristicType
        }

        struct Confirmation {
            let ephemeralCharacteristicPointer: CharacteristicType
            let all: CharacteristicCollection
        }

        let link: Link
        // TODO: IPD-160379 These properties must be made non optional when GGHost macOS supports these characteristics.
        let linkStatus: LinkStatus?
        let confirmation: Confirmation?
    }

    /// Iterates the discovered services and attempts to create a collection of all the required and optional characteristics
    /// needed to resolve a hub connection.
    func ensureExpectedCharacteristics(inServices services: [BluetoothConnector.DiscoveredService]) -> Single<ExpectedCharacteristics> {
        Single.deferred {
            // Link service
            let link: Single<ExpectedCharacteristics.Link>

            do {
                guard
                    let serviceConfiguration = self.configuration.linkService
                        .first(where: { candidateConfiguration in services.contains { $0.uuid == candidateConfiguration.serviceUUID } })
                else {
                    let expectedServiceUuids = self.configuration.linkService.map { $0.serviceUUID }
                    LogError("Hub: Required service not found: \(expectedServiceUuids)", domain: .bluetooth)
                    throw PeripheralError.serviceNotFound(expectedServiceUuids)
                }

                let allCharacteristics = try services.required(serviceConfiguration.serviceUUID).characteristics

                link = allCharacteristics.map { characteristics in
                    ExpectedCharacteristics.Link(
                        rx: try characteristics.required(serviceConfiguration.rxUUID),
                        tx: try characteristics.required(serviceConfiguration.txUUID)
                    )
                }
            }

            // Link status service
            let linkStatus: Single<ExpectedCharacteristics.LinkStatus?>

            linkStatus:do {
                let serviceConfiguration = self.configuration.linkStatusService

                guard
                    let allCharacteristics = services.optional(serviceConfiguration.serviceUUID)?.characteristics
                else {
                    linkStatus = .just(nil)
                    break linkStatus
                }

                linkStatus = allCharacteristics.map { characteristics in
                    ExpectedCharacteristics.LinkStatus(
                        currentConnectionConfiguration: try characteristics.required(serviceConfiguration.currentConnectionConfigurationUUID),
                        currentConnectionStatus: try characteristics.required(serviceConfiguration.currentConnectionStatusUUID),
                        bondSecure: try characteristics.required(serviceConfiguration.bondSecureUUID)
                    )
                }
            }

            // Confirmation service
            let confirmation: Single<ExpectedCharacteristics.Confirmation?>

            confirmation:do {
                let serviceConfiguration = self.configuration.confirmationService

                guard let allCharacteristics = services.optional(serviceConfiguration.serviceUUID)?.characteristics else {
                    confirmation = .just(nil)
                    break confirmation
                }

                confirmation = allCharacteristics.map { characteristics in
                    ExpectedCharacteristics.Confirmation(
                        ephemeralCharacteristicPointer: try characteristics.required(serviceConfiguration.ephemeralCharacteristicPointerUUID),
                        all: characteristics
                    )
                }
            }

            return Single.zip(link, linkStatus, confirmation)
                .map { ExpectedCharacteristics(link: $0.0, linkStatus: $0.1, confirmation: $0.2) }
        }
    }
}

// MARK: Ephemeral
extension Hub {
    func writeEphemeralCharacteristic(
        ephemeralCharacteristicPointer: CharacteristicType,
        confirmationCharacteristics: CharacteristicCollection
    ) -> Completable {
        ephemeralCharacteristicPointer.readValue()
            .asObservable()
            .compactMap { characteristic -> CharacteristicType? in
                let value = characteristic.value

                guard let ephemeralPointerData = value else {
                    throw HubError.illegalEphemeralCharacteristicPointer(value)
                }

                let ephemeralCharacteristicUUID: CBUUID

                do {
                    ephemeralCharacteristicUUID = try CBUUID(arbitraryData: ephemeralPointerData)
                } catch {
                    LogError("Hub: Invalid ephemeral pointer \(error)", domain: .bluetooth)
                    throw HubError.illegalEphemeralCharacteristicPointer(ephemeralPointerData)
                }

                LogBluetoothInfo("Hub: Ephemeral characteristic UUID: \(ephemeralCharacteristicUUID)")

                return confirmationCharacteristics.optional(ephemeralCharacteristicUUID)
            }
            .flatMap { ephemeralCharacteristic -> Single<CharacteristicType> in
                LogBluetoothInfo("Hub: Writing 0x00 to ephemeral characteristic")

                return ephemeralCharacteristic.writeValue(Data([0x00]), type: .withResponse)
            }
            .ignoreElements()
    }
}

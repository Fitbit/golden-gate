//
//  BluetoothConnector.swift
//  BluetoothConnection
//
//  Created by Vlad Corneci on 04/08/2020.
//  Copyright © 2020 Fitbit. All rights reserved.
//

import CoreBluetooth
import Foundation
import Rxbit
import RxBluetoothKit
import RxSwift

public enum BluetoothConnectorMetricsEvent: MetricsEvent, Equatable {
    case stateChangedToConnecting
    case stateChangedToConnected
    case stateChangedToDisconnected
    case stateChangedToDiscoveredServices
    case stateChangedToDiscoveredCharacteristics
}

/// A general use BLE connector that connects to a peripheral and discovers its services and characteristics.
final public class BluetoothConnector: Connector, Instrumentable {
    private let centralManager: CentralManagerType
    private let scheduler: SchedulerType

    private let metricsSubject = PublishSubject<BluetoothConnectorMetricsEvent>()
    public var metrics: Observable<BluetoothConnectorMetricsEvent> { metricsSubject.asObservable() }

    public init(centralManager: CentralManagerType, scheduler: SchedulerType) {
        self.centralManager = centralManager
        self.scheduler = scheduler
    }

    /// Establishes the connection with a BLE `Peripheral` identified by a peripheral UUID descriptor.
    /// - Parameter descriptor: The UUID that indentifies the peripheral to connect to.
    ///
    /// - Returns: Observable
    ///   - Value: The peripheral connection status. When the connection is established, the status contains a connection,
    ///   which is an array of discovered services. The services hold observables that discover their characteristics
    ///   when subscribed to.
    ///   - Error: If the connection attempt fails.
    ///   - Complete: Never
    public func establishConnection(descriptor: PeerDescriptor) -> Observable<ConnectionStatus<[DiscoveredService]>> {
        centralManager.establishConnection(identifier: descriptor.identifier, scheduler: scheduler)
            .do(onNext: { status in
                switch status {
                case .connecting: self.metricsSubject.onNext(.stateChangedToConnecting)
                case .connected: self.metricsSubject.onNext(.stateChangedToConnected)
                case .disconnected: self.metricsSubject.onNext(.stateChangedToDisconnected)
                }
            })
            .flatMapLatest { status -> Observable<ConnectionStatus<[DiscoveredService]>> in
                status.mapObservable { peripheral in self.discoverServices(peripheral: peripheral) }
            }
    }

    private func discoverServices(peripheral: PeripheralType) -> Observable<[DiscoveredService]> {
        let services = peripheral.discoverServices(nil)
            .logInfo("[BluetoothConnector] Discover services:", .bluetooth, .next)
            .do(onNext: { _ in self.metricsSubject.onNext(.stateChangedToDiscoveredServices) })

        // When the service database is modified, all CoreBlueooth services and characteristics
        // become invalid. To keep things straightforward and avoid nasty bugs, it was decided
        // to tear down the current connection.
        let servicesChangedIndication = peripheral.observeServicesModification()
            .delayUnknownDisconnectionError(scheduler: scheduler)
            .logWarning("[BluetoothConnector] Received services change indication", .bluetooth, .next)
            .map { _ -> [ServiceType] in throw PeripheralError.servicesChanged }

        return Observable.merge(services, servicesChangedIndication)
            .map { [metricsSubject] in
                $0.map { service -> DiscoveredService in
                    let characteristics = service
                        .discoverCharacteristics(nil)
                        .map { CharacteristicCollection($0) }
                        .logInfo("[BluetoothConnector] Discover characteristics for service \(service.uuid):", .bluetooth, .next)
                        .do(onNext: { _ in metricsSubject.onNext(.stateChangedToDiscoveredCharacteristics) })
                        .asSingle()

                    return DiscoveredService(uuid: service.uuid, characteristics: characteristics)
                }
            }
    }
}

extension BluetoothConnector {
    /// A BLE service discovered by the Connector.
    public struct DiscoveredService {
        /// The UUID of the BLE service
        public let uuid: CBUUID

        /// The characteristics available under the BLE service. The characteristics are
        /// discovered on demand, when the single is subscribed, to avoid doing unnecessary
        /// work for services that are not of interest.
        public let characteristics: Single<CharacteristicCollection>

        public init(uuid: CBUUID, characteristics: Single<CharacteristicCollection>) {
            self.uuid = uuid
            self.characteristics = characteristics
        }
    }
}

extension Array where Element == BluetoothConnector.DiscoveredService {
    public func optional(_ uuid: CBUUID) -> Element? {
        guard let service = first(where: { $0.uuid == uuid }) else {
            LogWarning("Optional service not found: \(uuid)", domain: .bluetooth)
            return nil
        }

        return service
    }

    public func required(_ uuid: CBUUID) throws -> Element {
        guard let service = first(where: { $0.uuid == uuid }) else {
            LogError("Required service not found: \(uuid)", domain: .bluetooth)
            throw PeripheralError.serviceNotFound([uuid])
        }

        return service
    }
}

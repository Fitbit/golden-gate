//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  CentralManagerType.swift
//  BluetoothConnection
//
//  Created by Emanuel Jarnea on 18/11/2020.
//

import CoreBluetooth
import Foundation
import RxBluetoothKit
import RxSwift

/// CentralManagerType is a ReactiveX wrapper over Core Bluetooth's Central Manager allowing to
/// discover, connect to remote peripheral devices and more.
public protocol CentralManagerType: AnyObject {
    /// Returns list of the peripherals which are currently connected to the system and implement any of the specified services.
    func retrieveConnectedPeripherals(withServices serviceUUIDs: [CBUUID]) -> [PeripheralType]

    /// Returns list of peripherals by their identifiers which are known to the central manager.
    func retrievePeripherals(withIdentifiers identifiers: [UUID]) -> [PeripheralType]

    /// Scans for peripherals after subscription to returned observable.
    func scanForPeripherals(withServices serviceUUIDs: [CBUUID]?, options: [String: Any]?) -> Observable<ScannedPeripheralType>

    /// Establishes a connection to a peripheral upon subscription
    /// and cancels it upon disposal.
    ///
    /// - Note: Ensures Bluetooth-APIs are not called if Bluetooth stack
    ///         is not available right now, but will wait instead
    ///         for it to become available eventually.
    ///
    /// - Parameter identifier: The identifier of the peripheral to connect to.
    /// - Parameter scheduler: The scheduler to run the subscription on.
    ///
    /// - Returns: Observable
    ///   - Value: State of the connection attempt changed.
    ///   - Error: If the connection attempt fails,
    ///            if the identifier can not be associated with a peripheral,
    ///            or once a previously connected peripheral disconnects.
    ///   - Complete: Never
    func establishConnection(identifier: UUID, scheduler: SchedulerType) -> Observable<ConnectionStatus<PeripheralType>>

    /// Observable that emits non transient Bluetooth states (any state but unknown). Starts with the current state.
    func stabilizedState() -> Observable<BluetoothState>
}

public enum CentralManagerError: Error {
    /// Identifier not known to OS.
    /// Retrying with the same identifier won't help and should be avoided.
    case identifierUnknown
}

extension CentralManager: CentralManagerType {
    public func retrieveConnectedPeripherals(withServices serviceUUIDs: [CBUUID]) -> [PeripheralType] {
        retrieveConnectedPeripherals(withServices: serviceUUIDs) as [Peripheral] as [PeripheralType]
    }

    public func retrievePeripherals(withIdentifiers identifiers: [UUID]) -> [PeripheralType] {
        retrievePeripherals(withIdentifiers: identifiers) as [Peripheral] as [PeripheralType]
    }

    public func scanForPeripherals(
        withServices serviceUUIDs: [CBUUID]?,
        options: [String: Any]?
    ) -> Observable<ScannedPeripheralType> {
        let peripherals: Observable<ScannedPeripheral> = scanForPeripherals(
            withServices: serviceUUIDs,
            options: options
        )

        return peripherals.map { $0 as ScannedPeripheralType }
    }

    public func stabilizedState() -> Observable<BluetoothState> {
        // Per Apple's docs, 'unknown' is a transient state, update being imminent.
        // Skip unknown state and wait for an update.
        observeStateWithInitialValue().filter { $0 != .unknown }.distinctUntilChanged()
    }
}

extension CentralManagerType {
    public func establishConnection(identifier: UUID, scheduler: SchedulerType) -> Observable<ConnectionStatus<PeripheralType>> {
        return stabilizedState()
            .logInfo("CentralManager.state:", .bluetooth, .next)
            .flatMapLatest { state -> Observable<ConnectionStatus<PeripheralType>> in
                guard state == .poweredOn else { throw BluetoothError(state: state) ?? BluetoothError.bluetoothInUnknownState }

                let knownPeripherals = self.retrievePeripherals(withIdentifiers: [identifier])

                guard let peripheral = knownPeripherals.first else {
                    throw CentralManagerError.identifierUnknown
                }

                return self.establishConnection(peripheral: peripheral)
                    .delayUnknownDisconnectionError(scheduler: scheduler)
                    .logDebug("CentralManager: retrievePeripherals(withIdentifiers: \(identifier))", .bluetooth, [.subscribe, .next])
            }
            .do(onSubscribe: { LogBluetoothInfo("CentralManager: Connecting to peripheral \(identifier)...") })
            .do(onNext: { LogBluetoothInfo("CentralManager: Connecting to peripheral: \($0)") })
            .do(onError: { LogBluetoothWarning("CentralManager: Error connecting to peripheral \(identifier): \($0)") })
            .distinctUntilChanged()
    }

    private func establishConnection(peripheral: PeripheralType) -> Observable<ConnectionStatus<PeripheralType>> {
        var options: [String: Any] = [:]
        // Required ANCS for all iOS 13+ versions in order to show the alert
#if os(iOS)
        if #available(iOS 13.0, *) {
            options[CBConnectPeripheralOptionRequiresANCS] = true
        }
#endif
        return peripheral
            .establishConnection(options: options)
            .map { ConnectionStatus.connected($0) }
            .startWith(.connecting)
    }
}

public extension ObservableType {
    /// Catch and delay `BluetoothError.peripheralDisconnected` errors that have a nil underlying error.
    ///
    /// - Remark: `CoreBluetooth` does not guarantee the order of events when Bluetooth is turned off. In some cases,
    /// central/peripheral managers are updated with the new state and then peripherals are disconnected. In other cases,
    /// the order is reversed. It may be useful to capture the information that some disconnection occurred due to
    /// Bluetooth being turned off. The proposed workaround is to delay the specific `BluetoothError.peripheralDisconnected(_, nil)`
    /// error, hoping that a `BluetoothError.bluetoothPoweredOff` error follows quickly enough (via some other Observable chain).
    func delayUnknownDisconnectionError(scheduler: SchedulerType, delay: DispatchTimeInterval = .milliseconds(300)) -> Observable<Element> {
        catchError { error in
            switch error {
            case BluetoothError.peripheralDisconnected(_, nil):
                return Observable.error(error).delaySubscription(delay, scheduler: scheduler)
            default:
                throw error
            }
        }
    }
}

extension BluetoothError {
    public init?(state: BluetoothState) {
        switch state {
        case .unsupported:
            self = .bluetoothUnsupported
        case .unauthorized:
            self = .bluetoothUnauthorized
        case .poweredOff:
            self = .bluetoothPoweredOff
        case .unknown:
            self = .bluetoothInUnknownState
        case .resetting:
            self = .bluetoothResetting
        case .poweredOn:
            return nil
        }
    }
}

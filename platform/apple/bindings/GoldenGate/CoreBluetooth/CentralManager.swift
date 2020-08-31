//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  CentralManager.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 10/27/17.
//

import CoreBluetooth
import Foundation
import Rxbit
import RxBluetoothKit
import RxSwift

public enum CentralManagerError: Error {
    /// Identifier not known to OS.
    /// Retrying with the same identifier won't help and should be avoided.
    case identifierUnknown
}

/// Current state of the connection intent.
public enum PeripheralConnectionStatus: Equatable {
    /// Not trying to connect at the moment (Bluetooth off).
    case disconnected

    /// Trying to find and connect to the device at the moment.
    case connecting

    /// Connected to the given device at the moment.
    case connected(Peripheral)
}

extension PeripheralConnectionStatus: CustomStringConvertible {
    public var description: String {
        switch self {
        case .disconnected: return "disconnected"
        case .connecting: return "connecting..."
        case .connected(let peripheral): return "connected (peripheral=\(peripheral ??? "nil"))"
        }
    }
}

/// Collection of helpers to connect to peripherals.
public class CentralManager {
    internal let manager: RxBluetoothKit.CentralManager
    private let queue: DispatchQueue
    private let scheduler: SerialDispatchQueueScheduler

    public init(manager: RxBluetoothKit.CentralManager, queue: DispatchQueue) {
        self.manager = manager
        self.queue = queue
        self.scheduler = .init(queue: queue, internalSerialQueueName: queue.label)
    }

    deinit {
        LogBluetoothDebug("CentralManager deinit")
    }

    /// Establishes a connection to a peripheral upon subscription
    /// and cancels it upon disposal.
    ///
    /// - Note: Ensures Bluetooth-APIs are not called if Bluetooth stack
    ///         is not available right now, but will wait instead
    ///         for it to become available eventually.
    ///
    /// - Parameter identifier: The identifier of the peripheral to connect to.
    ///
    /// - Returns: Observable
    ///   - Value: State of the connection attempt changed.
    ///   - Error: If the connection attempt fails,
    ///            if the identifier can not be associated with a peripheral,
    ///            or once a previously connected peripheral disconnects.
    ///   - Complete: Never
    public func peripheralConnectionStatus(for identifier: UUID) -> Observable<PeripheralConnectionStatus> {
        return manager
            .observeStateWithInitialValue()
            .distinctUntilChanged()
            .subscribeOn(scheduler)
            .do(onNext: { [weak self] state in
                LogBluetoothInfo("\(self ??? "CentralManager").rx_state: \(state)")
            })
            .flatMapLatest { [manager] state -> Observable<PeripheralConnectionStatus> in
                guard state == .poweredOn else { return .just(.disconnected) }

                let knownPeripherals = manager.retrievePeripherals(withIdentifiers: [identifier])

                guard let peripheral = knownPeripherals.first else {
                    throw CentralManagerError.identifierUnknown
                }

                return self.peripheralConnectionStatus(for: peripheral)
                    .logDebug("retrievePeripherals(withIdentifiers: \(identifier))", .bluetooth, [.subscribe, .next])
            }
            .observeOn(scheduler)
            .do(onSubscribe: { LogBluetoothInfo("Connecting to peripheral \(identifier)...") })
            .do(onNext: { LogBluetoothInfo("Connecting to peripheral: \($0)") })
            .do(onError: { LogBluetoothWarning("Error connecting to peripheral \(identifier): \($0)") })
            .startWith(.disconnected)
            .distinctUntilChanged()
    }

    private func peripheralConnectionStatus(for peripheral: Peripheral) -> Observable<PeripheralConnectionStatus> {
        var options: [String: Any] = [:]
        // Required ANCS for all iOS 13 versions in order to show the alert
        #if os(iOS)
            if #available(iOS 13.0, *) {
                options[CBConnectPeripheralOptionRequiresANCS] = true
            }
        #endif
        return peripheral
            .establishConnection(options: options)
            .map { PeripheralConnectionStatus.connected($0) }
            .startWith(.connecting)
    }
}

extension CBPeripheral {
    // NOTE: `CBConnection` was added in macOS 10.13 SDK.
    // Now Xcode believes that `identifier` did not exist before...
    //
    // https://forums.developer.apple.com/thread/84375
    var hackyIdentifier: UUID {
        // swiftlint:disable:next force_cast
        return value(forKey: "identifier") as! NSUUID as UUID
    }
}

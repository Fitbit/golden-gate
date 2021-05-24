//
//  BluetoothReconnectStrategy.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 2/26/18.
//  Copyright © 2018 Fitbit. All rights reserved.
//

import CoreBluetooth
import Foundation
import RxBluetoothKit
import RxRelay
import RxSwift

public class BluetoothReconnectStrategy: ReconnectStrategy {
    public let resetFailureHistory = PublishRelay<Void>()

    private let resumeTrigger: Observable<Void>
    private let retryDelay: DispatchTimeInterval

    private let observationInterval: TimeInterval = 2 * 60
    private let observationLimit = 5
    private var observations = [Observation]()

    private let disposeBag = DisposeBag()

    /// - Parameters:
    ///   - resume: Emit values whenever suspended reconnections attempts
    ///       should be resumed. See `ReconnectStrategy`. The values describe
    ///       the trigger.
    ///   - retryDelay: The time interval to wait before retrying the connection.
    public init(
        resumeTrigger: Observable<CustomStringConvertible>,
        retryDelay: DispatchTimeInterval = .milliseconds(250)
    ) {
        self.resumeTrigger = resumeTrigger
            .logInfo("Received trigger to resume reconnection:", .bluetooth, .next)
            .map { _ in () }

        self.retryDelay = retryDelay

        resetFailureHistory
            .subscribe(onNext: { [weak self] _ in
                self?.observations.removeAll()
            })
            .disposed(by: disposeBag)
    }

    public func action(for error: Error) -> ReconnectStrategyAction {
        switch error {
        case CentralManagerError.identifierUnknown, ConnectionResolverError.couldNotResolveConnection:
            observations.removeAll()
            return .stop(error)
        case BluetoothError.bluetoothPoweredOff,
             BluetoothError.bluetoothInUnknownState,
             BluetoothError.bluetoothResetting:
            observations.removeAll()
            return .suspendUntil(resumeTrigger)
        default:
            return actionFromPast(for: error) ?? actionFromCurrent(error: error)
        }
    }

    private func actionFromCurrent(error: Error) -> ReconnectStrategyAction {
        if #available(iOS 10, OSX 10.13, *) {
            switch error {
            case CBError.connectionLimitReached:
                return .suspendUntil(resumeTrigger)
            case CBError.connectionFailed:
                return .retryWithDelay(retryDelay)
            default: break
            }
        }

        switch error {
        case let error as BluetoothError where error.isHalfBondedError:
            // handle the case when we cannot connect to a peripheral on iOS 13.4+ (half bond mode)
            return .suspendUntil(resumeTrigger)
        case BluetoothError.peripheralConnectionFailed,
             BluetoothError.peripheralDisconnected,
             CBError.peripheralDisconnected,
             CBError.connectionTimeout,
             PeripheralError.servicesChanged:
            return .retryWithDelay(retryDelay)
        default:
            LogBluetoothError("Unexpected error in the reconnect strategy: \(error)")
            return .retryWithDelay(retryDelay)
        }
    }

    /// Makes a decision based on what has been observed in the past.
    private func actionFromPast(for error: Error) -> ReconnectStrategyAction? {
        let cutoffDate = Date(timeIntervalSinceNow: -observationInterval)

        // Sliding window - keep fresh observations and append new observation.
        observations = observations.filter { $0.date >= cutoffDate }
        observations.append(Observation(error: error, date: Date()))

        guard observations.count >= observationLimit else {
            return nil
        }

        LogBluetoothWarning("Observed \(observations.count) errors since \(cutoffDate).")

        // Forget everything that happened, so that once reconnections
        // are resumed, we allow for retries again.
        observations.removeAll()

        return .suspendUntil(resumeTrigger)
    }

    private struct Observation {
        let error: Error
        let date: Date
    }
}

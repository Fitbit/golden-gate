//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  BluetoothReconnectStrategy.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 2/26/18.
//

import CoreBluetooth
import Foundation
import RxBluetoothKit
import RxSwift

public class BluetoothReconnectStrategy: ReconnectStrategy {
    private let resumeTrigger: Observable<Void>

    private let observationInterval: TimeInterval = 2 * 60
    private let observationLimit = 3
    private var observations = [Observation]()

    private let disposeBag = DisposeBag()

    /// - Parameters:
    ///   - resume: Emit values whenever suspended reconnections attempts
    ///       should be resumed. See `ReconnectStrategy`.
    ///   - bluetoothState: The operating system's Bluetooth state which
    ///       will be used to also resume suspended reconnections
    ///       if an edge is detected.
    ///   - resetFailureHistory: Resets the failure history when a new
    ///       event is emmited.
    public init(
        resumeTrigger: Observable<Void>,
        bluetoothState: Observable<BluetoothState>,
        resetFailureHistory: Observable<Void>
    ) {
        // Emit true if bluetooth is enabled, false otherwise
        let isBluetoothEnabled = bluetoothState
            .map { (state) -> Bool in
                switch state {
                case .poweredOff, .resetting, .unauthorized, .unknown, .unsupported:
                    return false
                case .poweredOn:
                    return true
                }
            }

        // Emit a value whenever we observe * -> PoweredOn
        // which might indicate the user power-cycling iOS's Bluetooth.
        let didEnableBluetoothTrigger = isBluetoothEnabled
            .skip(1)
            .distinctUntilChanged()
            .filter { $0 }
            .map { _ in () }

        self.resumeTrigger = Observable.merge(
            resumeTrigger.do(onNext: { _ in LogBindingsInfo("Received Trigger to resume reconnection") }),
            didEnableBluetoothTrigger.do(onNext: { _ in LogBluetoothInfo("Detected Bluetooth Powering on") })
        )

        resetFailureHistory.subscribe(onNext: { [unowned self] _ in
            self.observations.removeAll()
        }).disposed(by: disposeBag)
    }

    public func action(for error: Error) -> ReconnectStrategyAction {
        switch error {
        case PeripheralError.invalidProtocolDetected:
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
        let delay: DispatchTimeInterval = .milliseconds(250)

        if #available(iOS 10, OSX 10.13, *) {
            switch error {
            case CBError.connectionLimitReached:
                return .suspendUntil(resumeTrigger)
            case CBError.connectionFailed:
                return .retryWithDelay(delay)
            default: break
            }
        }

        switch error {
        case BluetoothError.peripheralConnectionFailed,
             BluetoothError.peripheralDisconnected,
             CBError.peripheralDisconnected,
             CBError.connectionTimeout:
            return .retryWithDelay(delay)
        default:
            LogBindingsWarning("Unexpected error: \(error)")
            return .retryWithDelay(delay)
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

        LogBindingsWarning("Observed \(observations.count) errors since \(cutoffDate).")

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

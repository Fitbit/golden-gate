//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  ConnectivityRetryStrategy.swift
//  BluetoothConnection
//
//  Created by Marcel Jackwerth on 2/26/18.
//

import CoreBluetooth
import Foundation
import Rxbit
import RxBluetoothKit
import RxRelay
import RxSwift

public final class ConnectivityRetryStrategy: RetryStrategy {
    private let configuration: RetryDelayConfiguration
    private let resetFailureHistoryRelay = PublishRelay<Void>()

    private let observationInterval: TimeInterval = 2 * 60
    private let observationLimit = 5
    private var observations = [Observation]()

    private let disposeBag = DisposeBag()

    public init(configuration: RetryDelayConfiguration) {
        self.configuration = configuration

        resetFailureHistoryRelay
            .subscribe(onNext: { [weak self] _ in
                self?.observations.removeAll()
            })
            .disposed(by: disposeBag)
    }

    public func resetFailureHistory() {
        resetFailureHistoryRelay.accept(())
    }

    public func action(for error: Error) -> RetryStrategyAction {
        switch error {
        case CentralManagerError.identifierUnknown, ConnectionResolverError.couldNotResolveConnection:
            observations.removeAll()
            return .error(error)
        case BluetoothError.bluetoothPoweredOff,
             BluetoothError.bluetoothInUnknownState,
             BluetoothError.bluetoothResetting:
            observations.removeAll()
            return suspendIfSupported(error: error)
        default:
            return actionFromPast(for: error) ?? actionFromCurrent(error: error)
        }
    }

    private func actionFromCurrent(error: Error) -> RetryStrategyAction {
        if #available(iOS 10, OSX 10.13, *) {
            switch error {
            case CBError.connectionLimitReached:
                return suspendIfSupported(error: error)
            case CBError.connectionFailed:
                return .delay(configuration)
            default: break
            }
        }

        switch error {
        case let error as BluetoothError where error.isHalfBondedError:
            // handle the case when we cannot connect to a peripheral on iOS 13.4+ (half bond mode)
            return suspendIfSupported(error: error)
        case BluetoothError.peripheralConnectionFailed,
             BluetoothError.peripheralDisconnected,
             CBError.peripheralDisconnected,
             CBError.connectionTimeout,
             PeripheralError.servicesChanged:
            return .delay(configuration)
        default:
            LogBluetoothError("Unexpected error in the connectivity retry strategy: \(error)")
            return .delay(configuration)
        }
    }

    /// Makes a decision based on what has been observed in the past.
    private func actionFromPast(for error: Error) -> RetryStrategyAction? {
        let cutoffDate = Date(timeIntervalSinceNow: -observationInterval)

        // Sliding window - keep fresh observations and append new observation.
        observations = observations.filter { $0.date >= cutoffDate }
        observations.append(Observation(error: error, date: Date()))

        guard observations.count >= observationLimit else {
            return nil
        }

        LogBluetoothWarning("Observed \(observations.count) connectivity errors since \(cutoffDate).")

        // Forget everything that happened, so that once connectivity
        // is resumed, we allow for retries again.
        observations.removeAll()

        return suspendIfSupported(error: error)
    }

    /// Creates a retry action that suspends retries until the resume trigger emits, or errors out if the resume trigger is nil.
    private func suspendIfSupported(error: Error) -> RetryStrategyAction {
        if let resumeTrigger = configuration.resume {
            return .suspendUntil(
                resumeTrigger
                    .logInfo("Received trigger to resume connectivity:", .bluetooth, .next)
                    .map { _ in () }
                    .take(1)
                    .asSingle()
            )
        } else {
            return .error(error)
        }
    }

    private struct Observation {
        let error: Error
        let date: Date
    }
}

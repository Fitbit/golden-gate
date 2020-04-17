//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  ReconnectStrategy.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 8/6/18.
//

import RxSwift

public protocol ReconnectStrategy {
    /// Called when an error has terminated the connection
    /// - Parameter error: The error that terminated the connection
    /// - Returns: The action that should be performed
    func action(for error: Error) -> ReconnectStrategyAction
}

public enum ReconnectStrategyAction {
    /// Will retry after a while.
    case retryWithDelay(DispatchTimeInterval)

    /// Will stop retrying until a `trigger` emits an element
    case suspendUntil(Observable<Void>)

    /// Will not retry because of an unrecoverable error.
    case stop(Error)
}

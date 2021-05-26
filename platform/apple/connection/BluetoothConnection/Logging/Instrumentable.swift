//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  Instrumentable.swift
//  BluetoothConnection
//
//  Created by Emanuel Jarnea on 25/01/2021.
//

import RxSwift

/// A type that publishes metrics events which describe its internal state.
public protocol Instrumentable {
    associatedtype Event: MetricsEvent
    var metrics: Observable<Event> { get }
}

public protocol MetricsEvent {}

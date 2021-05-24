//
//  Instrumentable.swift
//  BluetoothConnection
//
//  Created by Emanuel Jarnea on 25/01/2021.
//  Copyright © 2021 Fitbit. All rights reserved.
//

import RxSwift

/// A type that publishes metrics events which describe its internal state.
public protocol Instrumentable {
    associatedtype Event: MetricsEvent
    var metrics: Observable<Event> { get }
}

public protocol MetricsEvent {}

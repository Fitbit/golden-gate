//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  ErrorTracker.swift
//  BluetoothConnection
//
//  Created by Emanuel Jarnea on 11/09/2020.
//

import Foundation

public protocol ErrorTracker {
    /// Records an event described by an Error object
    func record(error: Error)
}

public final class ErrorTrackers {
    public static var `default`: ErrorTracker?
}

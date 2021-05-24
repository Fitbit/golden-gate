//
//  ErrorTracker.swift
//  BluetoothConnection
//
//  Created by Emanuel Jarnea on 11/09/2020.
//  Copyright © 2020 Fitbit. All rights reserved.
//

import Foundation

public protocol ErrorTracker {
    /// Records an event described by an Error object
    func record(error: Error)
}

public final class ErrorTrackers {
    public static var `default`: ErrorTracker?
}

//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  LoggingErrorTracker.swift
//  GoldenGateHost
//
//  Created by Emanuel Jarnea on 11/09/2020.
//

import BluetoothConnection

final class LoggingErrorTracker: ErrorTracker {
    func record(error: Error) {
        Log("[ErrorTracker] \(error)", .error)
    }
}

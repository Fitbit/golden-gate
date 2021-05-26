//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  BluetoothConnectionError.swift
//  BluetoothConnection
//
//  Created by Vlad Corneci on 17/06/2020.
//

/// Generic `BluetoothConnection` errors.
public enum BluetoothConnectionError: Error {
    /// The receiving part would block. This error is usually thrown by data flow managing objects that
    /// provide an async API e.g. `DataSink`.
    case wouldBlock
}

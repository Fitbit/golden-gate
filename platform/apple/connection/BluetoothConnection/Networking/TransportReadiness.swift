//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  TransportReadiness.swift
//  BluetoothConnection
//
//  Created by Emanuel Jarnea on 10/09/2019.
//

import Foundation

public enum TransportReadiness {
    case ready
    case notReady(reason: Error)
}

//
//  TransportReadiness.swift
//  BluetoothConnection
//
//  Created by Emanuel Jarnea on 10/09/2019.
//  Copyright © 2019 Fitbit. All rights reserved.
//

import Foundation

public enum TransportReadiness {
    case ready
    case notReady(reason: Error)
}

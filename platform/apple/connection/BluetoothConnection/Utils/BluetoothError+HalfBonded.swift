//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  BluetoothError+HalfBonded.swift
//  BluetoothConnection
//
//  Created by Remus Leonte on 5/22/20.
//

import CoreBluetooth
import RxBluetoothKit

public extension BluetoothError {
    var isHalfBondedError: Bool {
        switch self {
        case .peripheralConnectionFailed(_, let error), .peripheralDisconnected(_, let error):
            return error?.isBluetoothHalfBondedError == true
        default: return false
        }
    }
}

public extension Error {
    var isBluetoothHalfBondedError: Bool {
        guard #available(iOS 13.4, OSX 10.15, *) else { return false }

        // NOTE: There's currently an anomaly with the half bonded error in CoreBluetooth: Apple creates a `CBATTError`
        // with `CBATTErrorDomain`, using a `CBError` code. Let's check for `CBError` with `CBErrorDomain` as well,
        // to account for a possible future fix.
        let halfBondedErrorDomains = [CBErrorDomain, CBATTErrorDomain]

        switch self {
        case let error as BluetoothError:
            return error.isHalfBondedError
        case let error as NSError where halfBondedErrorDomains.contains(error.domain) && error.code == CBError.peerRemovedPairingInformation.rawValue:
            return true
        default: return false
        }
    }
}


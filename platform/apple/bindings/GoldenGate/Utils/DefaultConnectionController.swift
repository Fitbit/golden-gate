//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  DefaultConnectionController.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 3/28/18.
//

/// The "standard" ConnectionController.
///
/// An auto-connecting BLE-based ConnectionController with a DTLS-Stack.
public typealias DefaultConnectionController =
    ReconnectingConnectionController<ConnectionController<BluetoothPeerDescriptor, Stack>>

//
//  DefaultConnectionController.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 3/28/18.
//  Copyright © 2018 Fitbit. All rights reserved.
//

/// The "standard" ConnectionController.
///
/// An auto-connecting BLE-based ConnectionController with a DTLS-Stack.
public typealias DefaultConnectionController =
    ReconnectingConnectionController<ConnectionController<BluetoothPeerDescriptor, Stack>>

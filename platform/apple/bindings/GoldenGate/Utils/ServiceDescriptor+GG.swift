//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  ServiceDescriptor+GG.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 4/11/18.
//

import BluetoothConnection

public extension ServiceDescriptor {
    /// Use BlasterService
    static let blasting = ServiceDescriptor(rawValue: "Blasting")

    /// Use CoAP server/client
    static let coap = ServiceDescriptor(rawValue: "CoAP")
}

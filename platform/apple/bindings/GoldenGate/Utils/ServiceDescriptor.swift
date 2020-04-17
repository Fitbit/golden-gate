//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  ServiceDescriptor.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 4/11/18.
//

/// Describes the services that should be launched on the `topPort`.
///
/// - See Also: `StackDescriptor`
public enum ServiceDescriptor: String, Equatable {
    /// Leave the `topPort` untouched.
    case none = "None"

    /// Use BlasterService
    case blasting = "Blasting"

    /// Use CoAP server/client
    case coap = "CoAP"
}

extension ServiceDescriptor: CustomStringConvertible {
    public var description: String {
        return rawValue
    }
}

//
//  ServiceDescriptor.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 4/11/18.
//  Copyright © 2018 Fitbit. All rights reserved.
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

//
//  ServiceDescriptor.swift
//  BluetoothConnection
//
//  Created by Emanuel Jarnea on 02/07/2020.
//  Copyright © 2020 Fitbit. All rights reserved.
//

/// Describes the services that should be launched on a peer's port.
public struct ServiceDescriptor: RawRepresentable, Equatable {
    public let rawValue: String

    public init(rawValue: String) {
        self.rawValue = rawValue
    }
}

extension ServiceDescriptor: CustomStringConvertible {
    /// Better description
    public var description: String {
        return rawValue
    }
}

extension ServiceDescriptor {
    /// Leave the port untouched.
    public static let none = ServiceDescriptor(rawValue: "None")
}

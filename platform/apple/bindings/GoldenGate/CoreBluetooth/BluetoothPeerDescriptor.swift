//
//  BluetoothPeerDescriptor.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 11/3/17.
//  Copyright © 2017 Fitbit. All rights reserved.
//

import Foundation

// TODO: Add documentation
public struct BluetoothPeerDescriptor: Hashable, Codable, CustomStringConvertible {
    public let identifier: UUID

    // TODO: Add Documentation
    public init(identifier: UUID) {
        self.identifier = identifier
    }

    public var description: String {
        return "(identifier=\(identifier))"
    }
}

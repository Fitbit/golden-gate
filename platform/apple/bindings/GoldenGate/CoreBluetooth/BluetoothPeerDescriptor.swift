//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  BluetoothPeerDescriptor.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 11/3/17.
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

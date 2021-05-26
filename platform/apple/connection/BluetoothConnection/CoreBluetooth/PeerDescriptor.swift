//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  PeerDescriptor.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 11/3/17.
//

import Foundation

/// Descriptor used by a bluetooth peripheral
public struct PeerDescriptor: Hashable, Codable, CustomStringConvertible {
    public let identifier: UUID

    public init(identifier: UUID) {
        self.identifier = identifier
    }

    public var description: String {
        return "(identifier=\(identifier))"
    }
}

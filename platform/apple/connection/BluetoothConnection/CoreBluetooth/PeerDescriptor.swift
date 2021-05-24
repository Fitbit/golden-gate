//
//  PeerDescriptor.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 11/3/17.
//  Copyright © 2017 Fitbit. All rights reserved.
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

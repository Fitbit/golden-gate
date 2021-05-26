//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  StackDescriptor.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 4/3/18.
//

/// Describes the stack structure.
public struct StackDescriptor: RawRepresentable, Equatable {
    public let rawValue: String

    public init(rawValue: String) {
        self.rawValue = rawValue
    }
}

extension StackDescriptor: CustomStringConvertible {
    /// Better description
    public var description: String {
        return rawValue
    }
}

extension StackDescriptor {
    /// Represents a stack that has no layers
    public static let empty = Self(rawValue: "∅")
}

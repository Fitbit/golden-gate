//
//  GGFourCharCode.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 3/28/18.
//  Copyright © 2018 Fitbit. All rights reserved.
//

/// Utility to deal with the issue that GG_4CC is not
/// imported by Swift's C parser.
public struct GGFourCharCode: RawRepresentable, ExpressibleByStringLiteral {
    public typealias RawValue = UInt32
    public let rawValue: RawValue

    /// Create a Four Char Code.
    public init(rawValue: RawValue) {
        self.rawValue = rawValue
    }

    /// Creates a Four Char Code from a string literal.
    public init(stringLiteral value: String) {
        precondition(value.count == 4, "Four Char Code requires four characters")

        let data = value.data(using: .ascii)!

        let rawValue = data
            .reversed() /// Reverse for easier offset math below
            .enumerated() /// Enumerate for offset math
            .reduce(0 as UInt32, { (accumulator, item) in
                let (offset, element) = item
                return accumulator | (UInt32(element) << (offset * 8))
            })

        self.rawValue = rawValue
    }
}

extension GGFourCharCode: CustomStringConvertible {
    public var description: String {
        var data = Data()
        data.append(bigEndian: rawValue)

        let characters = data.asBigEndians().map { Character(UnicodeScalar($0)) }

        return String(characters)
    }
}


//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  IpAddress.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 3/28/18.
//

import Foundation
import GoldenGateXP

/// An IP Address.
///
/// - See Also: `GG_IpAddress`
public struct IpAddress: CustomStringConvertible, Hashable {
    internal let gg: GG_IpAddress

    /// Creates an IP Address from a string.
    ///
    /// - See Also: `GG_IpAddress_SetFromString`
    public init(string: String) throws {
        var address = GG_IpAddress()

        try string.data(using: .utf8)!.withUnsafeBytes { (bytes: UnsafeRawBufferPointer) in
            try GG_IpAddress_SetFromString(
                &address,
                bytes.baseAddress?.assumingMemoryBound(to: Int8.self)
            ).rethrow()
        }

        gg = address
    }

    /// Creates an IP Address from an `in_addr`.
    ///
    /// - See Also: `GG_IpAddress_SetFromInteger`
    internal init(in_addr: in_addr) { // swiftlint:disable:this identifier_name
        var address = GG_IpAddress()

        GG_IpAddress_SetFromInteger(&address, in_addr.s_addr.bigEndian)

        gg = address
    }

    /// Creates an IP Address from a `GG_IpAddress`
    internal init(_ gg: GG_IpAddress) {
        self.gg = gg
    }

    /// The IP Address as a standard string of the format "123.123.123.123"
    ///
    /// - See Also: `GG_IpAddress_AsString`
    public var description: String {
        let bufferSize = "xxx.xxx.xxx.xxx".count
        var buffer = Data(count: bufferSize)
        var mutableCopy = gg

        let string = buffer.withUnsafeMutableBytes { (bytes: UnsafeMutableRawBufferPointer) -> String in
            let pointer = bytes.baseAddress?.assumingMemoryBound(to: Int8.self)
            GG_IpAddress_AsString(&mutableCopy, pointer, bufferSize)
            return String(cString: pointer!)
        }

        return string
    }

    public static func == (lhs: IpAddress, rhs: IpAddress) -> Bool {
        var ggLhs = lhs.gg
        var ggRhs = rhs.gg
        return GG_IpAddress_Equal(&ggLhs, &ggRhs)
    }

    public func hash(into hasher: inout Hasher) {
        var mutableCopy = gg
        hasher.combine(Int(GG_IpAddress_AsInteger(&mutableCopy)))
    }

    public static var any = IpAddress(GG_IpAddress_Any)
}

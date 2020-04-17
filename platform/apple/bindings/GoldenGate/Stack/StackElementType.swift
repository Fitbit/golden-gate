//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  StackElementType.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 4/6/18.
//

import Foundation

/// Wrapper for `GG_STACK_ELEMENT_TYPE_*` values.
///
/// Sadly the `GG_4CC` macro is not imported from XP,
/// so this is a manual
internal struct StackElementType: RawRepresentable {
    typealias RawValue = GGFourCharCode
    let rawValue: RawValue

    /// - See Also: GG_STACK_ELEMENT_TYPE_ACTIVITY_MONITOR
    static let activityMonitor = StackElementType(rawValue: "amon")

    /// - See Also: GG_STACK_ELEMENT_TYPE_GATTLINK
    static let gattlink = StackElementType(rawValue: "glnk")

    /// - See Also: GG_STACK_ELEMENT_TYPE_IP_NETWORK_INTERFACE
    static let ipNetworkInterface = StackElementType(rawValue: "neti")

    /// - See Also: GG_STACK_ELEMENT_TYPE_DATAGRAM_SOCKET
    static let datagramSocket = StackElementType(rawValue: "udps")

    /// - See Also: GG_STACK_ELEMENT_TYPE_DTLS_CLIENT
    static let dtlsClient = StackElementType(rawValue: "tlsc")

    /// - See Also: GG_STACK_ELEMENT_TYPE_DTLS_SERVER
    static let dtlsServer = StackElementType(rawValue: "tlss")
}

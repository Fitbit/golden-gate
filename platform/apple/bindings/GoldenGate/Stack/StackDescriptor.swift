//
//  StackDescriptor.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 4/3/18.
//  Copyright © 2018 Fitbit. All rights reserved.
//

/// Describes the stack structure.
public struct StackDescriptor: RawRepresentable, Equatable {
    /// Elements of a Stack Descriptor
    public enum Element: Character {
        /// A network interface
        case netif = "N"

        /// A gattlink element
        case gattlink = "G"

        /// A socket
        case socket = "S"

        /// A DTLS transport
        case dtls = "D"

        /// A Activity Monitor
        case activity = "A"
    }

    public let rawValue: String

    public init(rawValue: String) {
        self.rawValue = rawValue
    }

    private init(_ elements: Element...) {
        assert(!elements.isEmpty)
        self.rawValue = String(elements.map { $0.rawValue })
    }

    /// - See Also: `GG_STACK_DESCRIPTOR_GATTLINK`
    public static let gattlink = StackDescriptor(.gattlink)

    /// - See Also: `GG_STACK_DESCRIPTOR_NETIF_GATTLINK`
    public static let netifGattlink = StackDescriptor(.netif, .gattlink)

    /// - See Also: `GG_STACK_DESCRIPTOR_SOCKET_NETIF_GATTLINK`
    public static let socketNetifGattlink = StackDescriptor(.socket, .netif, .gattlink)

    /// - See Also: `GG_STACK_DESCRIPTOR_DTLS_SOCKET_NETIF_GATTLINK`
    public static let dtlsSocketNetifGattlink = StackDescriptor(.dtls, .socket, .netif, .gattlink)

    /// - See Also: `GG_STACK_DESCRIPTOR_DTLS_SOCKET_NETIF_GATTLINK_ACTIVITY`
    public static let dtlsSocketNetifGattlinkActivity = StackDescriptor(.dtls, .socket, .netif, .gattlink, .activity)

    /// True, if the descriptor asks for Activity Monitor
    public var hasActivityMonitor: Bool {
        return rawValue.contains("A")
    }

    /// True, if descriptor asks for Gattlink
    internal var hasGattlink: Bool {
        return rawValue.contains("G")
    }

    /// True, if descriptor asks for DTLS (server or client)
    internal var hasDtls: Bool {
        return rawValue.contains("D")
    }

    /// True, if the descriptor asks for a Network Interface
    public var hasNetworkInterface: Bool {
        return rawValue.contains("N")
    }

    /// True if the descriptor asks for any encryption mechanisms.
    public var isSecure: Bool {
        return hasDtls
    }
}

extension StackDescriptor: CustomStringConvertible {
    /// Better description
    public var description: String {
        return rawValue
    }
}

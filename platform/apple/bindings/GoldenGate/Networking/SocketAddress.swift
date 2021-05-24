//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  SocketAddress.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 11/30/17.
//

import CoreFoundation
import Darwin
import GoldenGateXP

// swiftlint:disable:next superfluous_disable_command
// swiftlint:disable identifier_name

public struct SocketAddress {
    internal let gg: GG_SocketAddress
    
    public init(_ gg: GG_SocketAddress) {
        self.gg = gg
    }

    public init(address: IpAddress, port: UInt16) {
        assert(port > 0)
        self.gg = GG_SocketAddress(address: address.gg, port: port)
    }

    /// Tries to create a socket address from an URL
    ///
    /// - Warning: Might perform DNS resolution on the same thread.
    public init?(url: URL) {
        var hints = addrinfo()
        hints.ai_socktype = SOCK_DGRAM

        // Limit to IPv4 matches
        hints.ai_family = AF_INET

        var servname = url.scheme

        if let port = url.port {
            hints.ai_flags = AI_NUMERICSERV
            servname = String(port)
        }

        var addrinfoRef: UnsafeMutablePointer<addrinfo>?
        guard
            getaddrinfo(url.host, servname, nil, &addrinfoRef) == 0,
            let addrinfo = addrinfoRef?.pointee
        else {
            LogBindingsError("Could not resolve: \(url)")
            return nil
        }

        defer { freeaddrinfo(addrinfoRef) }

        let addresses = Set(addrinfo.compactMap { addrinfo -> SocketAddress? in
            // Limit to IPv4 matches
            guard addrinfo.ai_family == AF_INET else { return nil }

            let (port, inAddr) = addrinfo.ai_addr.withMemoryRebound(to: sockaddr_in.self, capacity: 1) { addr in
                (addr.pointee.sin_port.bigEndian, addr.pointee.sin_addr)
            }

            guard port > 0 else { return nil }

            let ipAddress = IpAddress(in_addr: inAddr)
            return SocketAddress(address: ipAddress, port: port)
        })

        guard let address = addresses.first else {
            LogBindingsError("Could not find an IPv4 address for: \(url)")
            return nil
        }

        if addresses.count > 1 {
            LogBindingsWarning("Multiple addresses found for \(url): \(addresses), picked: \(address)")
        }

        self.gg = address.gg
    }

    var address: IpAddress {
        return IpAddress(gg.address)
    }

    var port: UInt16 {
        return gg.port
    }
}

extension SocketAddress: CustomStringConvertible {
    public var description: String {
        return "\(address):\(port)"
    }
}

extension SocketAddress: Hashable {
    public func hash(into hasher: inout Hasher) {
        hasher.combine(address)
        hasher.combine(port)
    }

    public static func == (lhs: SocketAddress, rhs: SocketAddress) -> Bool {
        return lhs.port == rhs.port && lhs.address == rhs.address
    }
}

extension addrinfo: Sequence {
    public func makeIterator() -> AnyIterator<addrinfo> {
        var cursor: addrinfo? = self

        return AnyIterator {
            guard let info = cursor else { return nil }
            cursor = info.ai_next?.pointee
            return info
        }
    }
}

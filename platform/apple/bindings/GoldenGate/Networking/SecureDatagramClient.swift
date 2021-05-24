//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  SecureDatagramSocket.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 12/21/17.
//

import GoldenGateXP

public class SecureDatagramClient {
    public struct Options {
        public let base: DtlsOptions
        public var pskIdentity: NSData
        public var psk: NSData
        public var ticket: NSData?

        public init(base: DtlsOptions, pskIdentity: Data, psk: Data, ticket: Data? = nil) {
            self.base = base
            self.pskIdentity = pskIdentity as NSData
            self.psk = psk as NSData
            self.ticket = ticket as NSData?
        }
    }

    // Utility to hold reference to the Swift objects
    internal class OptionsContainer {
        private var options: Options

        internal var gg: UnsafeHeapAllocatedValue<GG_TlsClientOptions>

        internal init(options: Options) {
            self.options = options

            let pskIdentityRef = self.options.pskIdentity.bytes.assumingMemoryBound(to: UInt8.self)
            let pskRef = self.options.psk.bytes.assumingMemoryBound(to: UInt8.self)
            let ticket = self.options.ticket?.bytes.assumingMemoryBound(to: UInt8.self)
    
            self.gg = UnsafeHeapAllocatedValue(
                GG_TlsClientOptions(
                    base: self.options.base.gg,
                    psk_identity: pskIdentityRef,
                    psk_identity_size: self.options.pskIdentity.length,
                    psk: pskRef,
                    psk_size: self.options.psk.length,
                    ticket: ticket,
                    ticket_size: self.options.ticket?.length ?? 0
                )
            )
        }

        internal var ref: UnsafeMutablePointer<GG_TlsClientOptions> {
            return gg.pointer
        }
    }

    private init() { }
}

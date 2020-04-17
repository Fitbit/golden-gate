//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  SecureDatagramServer.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 4/4/18.
//

import GoldenGateXP

public class SecureDatagramServer {
    public struct Options {
        public let base: DtlsOptions
        public let keyResolver: TlsKeyResolver

        public init(base: DtlsOptions, keyResolver: TlsKeyResolver) {
            self.base = base
            self.keyResolver = keyResolver
        }
    }

    // Utility to hold reference to the Swift objects
    internal class OptionsContainer {
        private var options: Options

        internal var gg: UnsafeHeapAllocatedValue<GG_TlsServerOptions>

        internal init(options: Options) {
            self.options = options

            self.gg = UnsafeHeapAllocatedValue(
                GG_TlsServerOptions(
                    base: options.base.gg,
                    key_resolver: options.keyResolver.ref
                )
            )
        }

        internal var ref: UnsafeMutablePointer<GG_TlsServerOptions> {
            return gg.pointer
        }
    }

    private init() { }
}

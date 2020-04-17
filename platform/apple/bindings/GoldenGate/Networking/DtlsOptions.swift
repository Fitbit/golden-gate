//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  DtlsOptions.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 4/4/18.
//

import GoldenGateXP

public struct DtlsOptions {
    public typealias CipherSuite = UInt16

    public static let defaultCipherSuites = DtlsOptions(cipherSuites: [
        CipherSuite(GG_TLS_PSK_WITH_AES_128_CCM),
        CipherSuite(GG_TLS_PSK_WITH_AES_128_GCM_SHA256),
        CipherSuite(GG_TLS_ECDHE_PSK_WITH_AES_128_CBC_SHA256)
    ])

    public let cipherSuites: [CipherSuite]

    public init(cipherSuites: [CipherSuite]) {
        self.cipherSuites = cipherSuites
    }

    internal var gg: GG_TlsOptions {
        return GG_TlsOptions(
            cipher_suites: cipherSuites,
            cipher_suites_count: cipherSuites.count
        )
    }
}

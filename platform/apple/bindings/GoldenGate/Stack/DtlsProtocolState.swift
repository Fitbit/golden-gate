//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  DtlsProtocolState.swift
//  GoldenGate
//
//  Created by Bogdan Vlad on 03/09/2018.
//

import Foundation
import GoldenGateXP

public enum DtlsProtocolState {
    /// Initial state after creation
    case initialized
    /// During the handshake phase
    case handshake
    /// After the handshake has been established
    case sessionEstablished(Data)
    /// After an error has occurred
    case error(Error)
}

extension DtlsProtocolState: Equatable {
    public static func == (lhs: DtlsProtocolState, rhs: DtlsProtocolState) -> Bool {
        switch (lhs, rhs) {
        case (.initialized, initialized),
             (.handshake, .handshake),
             (.sessionEstablished, .sessionEstablished),
             (.error, .error):
            return true
        default:
            return false
        }
    }
}

extension DtlsProtocolState {
    init(_ ggProtocolStatus: GG_DtlsProtocolStatus) {
        switch ggProtocolStatus.state {
        case GG_TLS_STATE_INIT:
            self = .initialized
        case GG_TLS_STATE_HANDSHAKE:
            self = .handshake
        case GG_TLS_STATE_SESSION:
            let identity = Data(bytes: ggProtocolStatus.psk_identity, count: ggProtocolStatus.psk_identity_size)
            self = .sessionEstablished(identity)
        case GG_TLS_STATE_ERROR:
            self = .error(GGRawError(ggProtocolStatus.last_error))
        default:
            preconditionFailure("Unhandled dtls protocol status")
        }
    }
}

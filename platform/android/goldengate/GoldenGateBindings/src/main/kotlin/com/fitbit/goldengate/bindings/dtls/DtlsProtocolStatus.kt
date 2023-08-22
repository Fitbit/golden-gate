// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bindings.dtls

class DtlsProtocolStatus(state: Int, val lastError: Int, val pskIdentity: String) {
    val state: TlsProtocolState = TlsProtocolState.getValue(state)

    enum class TlsProtocolState(val value: Int) {
        TLS_STATE_INIT(0),      ///< Initial state after creation
        TLS_STATE_HANDSHAKE(1), ///< During the handshake phase
        TLS_STATE_SESSION(2),   ///< After the handshake has completed
        TLS_STATE_ERROR(3),      ///< After an error has occurred
        TLS_STATE_UNKNOWN(-1);

        companion object {
            /**
             * Convert int value to [TlsProtocolState]
             */
            fun getValue(value: Int): TlsProtocolState {
                for (state in TlsProtocolState.values()) {
                    if (state.value == value) return state
                }
                return TLS_STATE_UNKNOWN
            }
        }
    }
}

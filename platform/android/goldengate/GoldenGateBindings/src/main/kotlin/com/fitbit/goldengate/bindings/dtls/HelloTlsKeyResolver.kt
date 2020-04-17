// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bindings.dtls

import com.fitbit.goldengate.bindings.node.NodeKey

internal val HELLO_KEY_ID = "hello".toByteArray()
internal val HELLO_KEY = byteArrayOf(
        0x00,
        0x01,
        0x02,
        0x03,
        0x04,
        0x05,
        0x06,
        0x07,
        0x08,
        0x09,
        0x0A,
        0x0B,
        0x0C,
        0x0D,
        0x0E,
        0x0F
)

/**
 * Static [TlsKeyResolver] for [HELLO_KEY_ID] key ID
 */
internal class HelloTlsKeyResolver : TlsKeyResolver() {

    override fun resolveKey(nodeKey: NodeKey<*>, keyId: ByteArray): ByteArray? {
        return when {
            keyId.contentEquals(HELLO_KEY_ID) -> HELLO_KEY
            else -> null
        }
    }
}

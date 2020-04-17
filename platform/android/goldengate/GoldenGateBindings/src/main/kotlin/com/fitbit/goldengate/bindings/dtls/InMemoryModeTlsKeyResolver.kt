// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bindings.dtls

import com.fitbit.goldengate.bindings.node.NodeKey

/**
 * Creates a Tls key resolver with given key id and key data, mainly for test mode
 *
 * @param keyId The pre-shared key identity
 * @param key   The pre-shared key with 16-byte length
 */
class InMemoryModeTlsKeyResolver(private val keyId: ByteArray, private val key: ByteArray) : TlsKeyResolver() {

    override fun resolveKey(nodeKey: NodeKey<*>, keyId: ByteArray): ByteArray? {
        return when {
            keyId.contentEquals(this.keyId) -> this.key
            else -> null
        }
    }
}

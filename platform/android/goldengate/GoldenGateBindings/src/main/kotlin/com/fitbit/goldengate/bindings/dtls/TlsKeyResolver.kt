// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bindings.dtls

import com.fitbit.goldengate.bindings.node.NodeKey
import com.fitbit.goldengate.bindings.util.hexString
import timber.log.Timber

abstract class TlsKeyResolver {

    internal var next: TlsKeyResolver? = null

    /**
     * Resolve a key (for example a pre-shared key) given a key identity.
     *
     * @param nodeKey node identifier this stack is attached to
     * @param keyId Identity of the key to resolve.
     * @return Resolved key for given key identifier if found, NULL otherwise
     */
    abstract fun resolveKey(nodeKey: NodeKey<*>, keyId: ByteArray): ByteArray?

    /**
     * Resolve a key (for example a pre-shared key) given a key identity.
     *
     * @param nodeKey node identifier this stack is attached to
     * @param keyId Identity of the key to resolve.
     * @return Resolved key for given key identifier if found, NULL otherwise
     */
    fun resolve(nodeKey: NodeKey<*>, keyId: ByteArray): ByteArray? {
        Timber.d("Resolving key for keyId: ${keyId.hexString()}")
        return resolveKey(nodeKey, keyId)?.let { key ->
            key
        } ?: next?.resolve(nodeKey, keyId)
    }
}

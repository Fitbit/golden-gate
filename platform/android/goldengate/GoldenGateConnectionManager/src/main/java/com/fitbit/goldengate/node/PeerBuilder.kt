package com.fitbit.goldengate.node

import com.fitbit.goldengate.bindings.node.NodeKey
import com.fitbit.goldengate.bindings.stack.StackService

/**
 * Builds a [Peer] with [StackService] of type [S] and key of type [K] and determines if an existing [Peer] is the same as the one's it builds
 */
interface PeerBuilder<S: StackService, K: NodeKey<*>> {

    /**
     * Builds a [Peer]
     */
    fun build(key: K) : Peer<S>

    /**
     * Tests if the given [Peer] is the same as the one this can [build]
     *
     * @param peer the [Peer] which is being tested to be the same as the one this will [build]
     */
    fun doesBuild(peer: Peer<*>): Boolean
}

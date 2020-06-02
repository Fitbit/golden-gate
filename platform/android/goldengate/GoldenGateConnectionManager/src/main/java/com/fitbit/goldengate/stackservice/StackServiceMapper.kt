// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.stackservice

import com.fitbit.goldengate.bindings.node.NodeKey
import com.fitbit.goldengate.bindings.stack.StackService
import com.fitbit.goldengate.node.Peer
import com.fitbit.goldengate.node.PeerBuilder
import com.fitbit.goldengate.node.NodeMapper

/**
 * Retrieves a [Peer] from a [NodeMapper] for a given [NodeKey] and extracts the [StackService] with [get].
 * The [NodeMapper] will build a [Peer] with the [PeerBuilder] given by [nodeBuilderProvider] if necessary
 *
 * @param nodeMapper The [NodeMapper] where all nodes are held
 * @param getStackService function to extract the [StackService] from a given [Peer]
 */
class StackServiceMapper<T: StackService, K: NodeKey<*>> (
    private val getStackService: (Peer<*>) -> T,
    private val peerBuilder: PeerBuilder<T, K>,
    private val nodeMapper: NodeMapper = NodeMapper.instance
) {

    /**
     * Get a [StackService] for a given [nodeKey]
     *
     * @param nodeKey the [NodeKey] of the desired [Peer] associated with the [StackService]
     */
    fun get(nodeKey: K) : T = getStackService(nodeMapper.get(nodeKey, peerBuilder))
}

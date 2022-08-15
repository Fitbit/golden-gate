// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.node

import com.fitbit.goldengate.bindings.node.NodeKey
import com.fitbit.goldengate.bindings.stack.StackService
import timber.log.Timber

/**
 * Maps a [NodeKey] to a [Node]. Uses a [PeerBuilder] to make the node if it does not exist or if
 * the existing [Node] is different than expected
 *
 * @param nodeMap a [MutableMap] mapping a [NodeKey] to a [Node]
 */
class NodeMapper internal constructor(
        private val nodeMap: MutableMap<NodeKey<*>, Peer<*>> = mutableMapOf()
) {

    companion object {
        val instance: NodeMapper by lazy { NodeMapper() }
    }

    /**
     * Tries to retrieve an existing [Node] with a [NodeKey] or builds one with the given [PeerBuilder]
     * if it does not exist or the [PeerBuilder] does not [build][PeerBuilder.doesBuild] it.
     *
     * @param nodeKey the [NodeKey] associated with the desired [Node]
     * @param peerBuilder the [PeerBuilder] used to build the [Node] if necessary a check if it
     * [builds][PeerBuilder.doesBuild] the same [Node]
     */
    @Synchronized
    fun <T: StackService, K: NodeKey<*>> get(nodeKey: K, peerBuilder: PeerBuilder<T, K>, forceRebuild: Boolean = false): Peer<*> =
        nodeMap[nodeKey].also {
            Timber.i("Looking up node with node key $nodeKey")
        }?.let { existingNode ->
            Timber.i("Node with key $nodeKey found")
            when {
                peerBuilder.doesBuild(existingNode) && !forceRebuild -> {
                    Timber.i("Node with key $nodeKey has the desired configuration")
                    existingNode
                }
                else -> {
                    Timber.i("Node with key $nodeKey has a different configuration than desired")
                    rebuildNode(existingNode, peerBuilder, nodeKey)
                }
            }
        }?: buildNode(peerBuilder, nodeKey)

    /**
     * Removes a [Node] from the map that has the specific [NodeKey],
     * this needs to be called when we close a Node as the existing one will be unusable
     *
     * @param nodeKey Key for the node to be removed
     */
    @Synchronized
    fun  <K: NodeKey<*>> removeNode(nodeKey:K){
        nodeMap[nodeKey]?.let {
            Timber.i("Removing the existing node with key $nodeKey")
            it.close()
            nodeMap.remove(nodeKey)
        }

    }

    /**
     * Build a [Node]
     */
    private fun <K: NodeKey<*>> buildNode(peerBuilder: PeerBuilder<*, K>, nodeKey: K): Peer<*> {
        Timber.i("Building node with node key $nodeKey")
        return peerBuilder.build(nodeKey).also { nodeMap[nodeKey] = it }
    }

    /**
     * Rebuild a [Node]. This is different from [build] as it [closes][Node.close] the
     * [existingNode] before [building][buildNode] a new one
     */
    private fun <K: NodeKey<*>> rebuildNode(existingNode: Peer<*>, peerBuilder: PeerBuilder<*, K>, nodeKey: K): Peer<*> {
        Timber.i("Closing the existing node with key $nodeKey")
        existingNode.close()
        Timber.i("Building new node with key $nodeKey")
        return buildNode(peerBuilder, nodeKey)
    }
}

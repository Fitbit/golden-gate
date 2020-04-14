package com.fitbit.goldengate.node

import com.fitbit.goldengate.bindings.node.NodeKey
import com.fitbit.goldengate.bindings.stack.StackService
import timber.log.Timber

/**
 * Maps a [NodeKey] to a [Node]. Uses a [NodeBuilder] to make the node if it does not exist or if
 * the existing [Node] is different than expected
 *
 * @param nodeMap a [MutableMap] mapping a [NodeKey] to a [Node]
 */
class NodeMapper internal constructor(
        private val nodeMap: MutableMap<NodeKey<*>, Node<*>> = mutableMapOf()
) {

    companion object {
        val instance: NodeMapper by lazy { NodeMapper() }
    }

    /**
     * Tries to retrieve an existing [Node] with a [NodeKey] or builds one with the given [NodeBuilder]
     * if it does not exist or the [NodeBuilder] does not [build][NodeBuilder.doesBuild] it.
     *
     * @param nodeKey the [NodeKey] associated with the desired [Node]
     * @param nodeBuilder the [NodeBuilder] used to build the [Node] if necessary a check if it
     * [builds][NodeBuilder.doesBuild] the same [Node]
     */
    @Synchronized
    fun <T: StackService, K: NodeKey<*>> get(nodeKey: K, nodeBuilder: NodeBuilder<T, K>, forceRebuild: Boolean = false): Node<*> =
        nodeMap[nodeKey].also {
            Timber.i("Looking up node with node key $nodeKey")
        }?.let { existingNode ->
            Timber.i("Node with key $nodeKey found")
            when {
                nodeBuilder.doesBuild(existingNode) && !forceRebuild -> {
                    Timber.i("Node with key $nodeKey has the desired configuration")
                    existingNode
                }
                else -> {
                    Timber.i("Node with key $nodeKey has a different configuration than desired")
                    rebuildNode(existingNode, nodeBuilder, nodeKey)
                }
            }
        }?: buildNode(nodeBuilder, nodeKey)

    /**
     * Removes a [Node] from the map that has the specific [NodeKey],
     * this needs to be called when we close a Node as the existing one will be unusable
     *
     * @param nodeKey Key for the node to be removed
     */
    @Synchronized
    fun  <K: NodeKey<*>> removeNode(nodeKey:K){
        nodeMap[nodeKey]?.let {
            Timber.i("Removing the exisiting node with key $nodeKey")
            it.close()
            nodeMap.remove(nodeKey)
        }

    }

    /**
     * Build a [Node]
     */
    private fun <K: NodeKey<*>> buildNode(nodeBuilder: NodeBuilder<*, K>, nodeKey: K): Node<*> {
        Timber.i("Building node with node key $nodeKey")
        return nodeBuilder.build(nodeKey).also { nodeMap[nodeKey] = it }
    }

    /**
     * Rebuild a [Node]. This is different from [build] as it [closes][Node.close] the
     * [existingNode] before [building][buildNode] a new one
     */
    private fun <K: NodeKey<*>> rebuildNode(existingNode: Node<*>, nodeBuilder: NodeBuilder<*, K>, nodeKey: K): Node<*> {
        Timber.i("Closing the existing node with key $nodeKey")
        existingNode.close()
        Timber.i("Building new node with key $nodeKey")
        return buildNode(nodeBuilder, nodeKey)
    }
}

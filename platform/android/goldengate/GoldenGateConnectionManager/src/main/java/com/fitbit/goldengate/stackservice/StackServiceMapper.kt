package com.fitbit.goldengate.stackservice

import com.fitbit.goldengate.bindings.node.NodeKey
import com.fitbit.goldengate.bindings.stack.StackService
import com.fitbit.goldengate.node.Node
import com.fitbit.goldengate.node.NodeBuilder
import com.fitbit.goldengate.node.NodeMapper

/**
 * Retrieves a [Node] from a [NodeMapper] for a given [NodeKey] and extracts the [StackService] with [get].
 * The [NodeMapper] will build a [Node] with the [NodeBuilder] given by [nodeBuilderProvider] if necessary
 *
 * @param nodeMapper The [NodeMapper] where all nodes are held
 * @param getStackService function to extract the [StackService] from a given [Node]
 */
class StackServiceMapper<T: StackService, K: NodeKey<*>> (
    private val getStackService: (Node<*>) -> T,
    private val nodeBuilder: NodeBuilder<T, K>,
    private val nodeMapper: NodeMapper = NodeMapper.instance
) {

    /**
     * Get a [StackService] for a given [nodeKey]
     *
     * @param nodeKey the [NodeKey] of the desired [Node] associated with the [StackService]
     */
    fun get(nodeKey: K) : T = getStackService(nodeMapper.get(nodeKey, nodeBuilder))
}

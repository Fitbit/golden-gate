package com.fitbit.goldengate.node

import com.fitbit.goldengate.bindings.node.NodeKey
import com.fitbit.goldengate.bindings.stack.StackService

/**
 * Builds a [Node] with [StackService] of type [S] and key of type [K] and determines if an existing [Node] is the same as the one's it builds
 */
interface NodeBuilder<S: StackService, K: NodeKey<*>> {

    /**
     * Builds a [Node]
     */
    fun build(key: K) : Node<S>

    /**
     * Tests if the given [Node] is the same as the one this can [build]
     *
     * @param node the [Node] which is being tested to be the same as the one this will [build]
     */
    fun doesBuild(node: Node<*>): Boolean
}

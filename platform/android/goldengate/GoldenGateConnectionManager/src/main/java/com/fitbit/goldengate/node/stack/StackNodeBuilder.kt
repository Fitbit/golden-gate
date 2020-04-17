// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.node.stack

import com.fitbit.goldengate.bindings.node.BluetoothAddressNodeKey
import com.fitbit.goldengate.bindings.stack.Stack
import com.fitbit.goldengate.bindings.stack.StackConfig
import com.fitbit.goldengate.bindings.stack.StackService
import com.fitbit.goldengate.node.Node
import com.fitbit.goldengate.node.NodeBuilder
import io.reactivex.Single

/**
 * Builds a [StackNode] with a [String] key
 *
 * @param clazz The [Class] of the [StackService] to use with this [StackNode]
 * @param stackConfig the [StackConfig] of the [StackNode]'s [Stack]
 * @param buildStackNode function for building a [StackNode]
 */
class StackNodeBuilder<T: StackService>(
        private val clazz: Class<in T>,
        private val stackConfig: StackConfig,
        private val buildStackNode: (BluetoothAddressNodeKey) -> StackNode<T>
): NodeBuilder<T, BluetoothAddressNodeKey> {

    /**
     * Utility constructor providing a default function to build a stack
     */
    internal constructor(
            clazz: Class<in T>,
            stackServiceProvider: () -> T,
            stackConfig: StackConfig
    ): this(clazz, stackConfig, { nodeKey ->
        StackNode(nodeKey, stackConfig, stackServiceProvider())
    })

    /**
     * Build the [StackNode], connect it and emit it via a [Single]
     */
    override fun build(key: BluetoothAddressNodeKey) : StackNode<T> = buildStackNode(key)

    override fun doesBuild(node: Node<*>): Boolean =
        node is StackNode
                && node.stackConfig == stackConfig
                && node.stackService::class.java == clazz
}

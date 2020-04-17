// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.stackservice

import com.fitbit.goldengate.bindings.node.NodeKey
import com.fitbit.goldengate.bindings.stack.StackService
import com.fitbit.goldengate.node.Node
import com.fitbit.goldengate.node.NodeBuilder
import com.fitbit.goldengate.node.NodeMapper
import com.fitbit.goldengate.node.stack.StackNode
import com.nhaarman.mockitokotlin2.doReturn
import com.nhaarman.mockitokotlin2.mock
import com.nhaarman.mockitokotlin2.verify
import org.junit.Test
import kotlin.test.assertEquals

class StackServiceMapperTest {

    private val nodeKey = mock<NodeKey<Any>>()
    private val nodeBuilder = mock<NodeBuilder<StackService, NodeKey<Any>>>()
    private val stackService = mock<StackService>()
    private val node = mock<StackNode<StackService>> {
        on { stackService } doReturn stackService
    }
    private val nodeMapper = mock<NodeMapper> {
        on { get(nodeKey, nodeBuilder) } doReturn node
    }
    private val getStackService = mock<(Node<*>) -> StackService> {
        on { invoke(node) } doReturn stackService
    }

    private val stackServiceMapper = StackServiceMapper(getStackService, nodeBuilder, nodeMapper)

    @Test
    fun get() {
        assertEquals(stackService, stackServiceMapper.get(nodeKey))

        verify(nodeMapper).get(nodeKey, nodeBuilder)
        verify(getStackService).invoke(node)
    }
}

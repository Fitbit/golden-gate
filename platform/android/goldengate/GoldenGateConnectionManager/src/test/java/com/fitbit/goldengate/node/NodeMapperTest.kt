package com.fitbit.goldengate.node

import com.fitbit.goldengate.bindings.node.NodeKey
import com.fitbit.goldengate.bindings.stack.StackService
import com.nhaarman.mockitokotlin2.doReturn
import com.nhaarman.mockitokotlin2.mock
import com.nhaarman.mockitokotlin2.times
import com.nhaarman.mockitokotlin2.verify
import org.junit.Test
import kotlin.test.assertEquals

class NodeMapperTest {

    private val node1 = mock<Node<StackService>>()
    private val node2 = mock<Node<StackService>>()

    private val nodeKey = mock<NodeKey<Unit>> {
        on { value } doReturn Unit
    }

    private val nodeBuilder1 = mock<NodeBuilder<StackService, NodeKey<Unit>>> {
        on { build(nodeKey) } doReturn node1
        on { doesBuild(node1) } doReturn true
        on { doesBuild(node2) } doReturn false
    }

    @Test
    fun getNodeBuildsNewNodeWhenNoNodes() {
        val nodeMap = mock<MutableMap<NodeKey<*>, Node<*>>>()
        assertEquals(node1, NodeMapper(nodeMap).get(nodeKey, nodeBuilder1))

        verify(nodeBuilder1, times(0)).doesBuild(node1)
        verify(nodeBuilder1).build(nodeKey)
        verify(nodeMap)[nodeKey] = node1
    }

    @Test
    fun getNodeRebuildsNodeWhenNotSame() {
        val nodeMap = mock<MutableMap<NodeKey<*>, Node<*>>> {
            on { get(nodeKey) } doReturn node2
        }

        assertEquals(node1, NodeMapper(nodeMap).get(nodeKey, nodeBuilder1))

        verify(node2).close()
        verify(nodeBuilder1).doesBuild(node2)
        verify(nodeBuilder1).build(nodeKey)
        verify(nodeMap)[nodeKey] = node1
    }

    @Test
    fun getNodeDoesNotBuildNewNodeWhenSame() {
        val nodeMap = mock<MutableMap<NodeKey<*>, Node<*>>> {
            on { get(nodeKey) } doReturn node1
        }

        assertEquals(node1, NodeMapper(nodeMap).get(nodeKey, nodeBuilder1))

        verify(nodeBuilder1).doesBuild(node1)
        verify(nodeBuilder1, times(0)).build(nodeKey)
        verify(nodeMap, times(0))[nodeKey] = node1
    }

    @Test
    fun removeNodeClosesTheNode() {
        val nodeMap = mock<MutableMap<NodeKey<*>, Node<*>>> {
            on { get(nodeKey) } doReturn node1
        }
        NodeMapper(nodeMap).removeNode(nodeKey)
        verify(node1).close()
        verify(nodeMap).remove(nodeKey)
    }
}

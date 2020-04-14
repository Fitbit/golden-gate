package com.fitbit.goldengate.node.stack

import com.fitbit.goldengate.bindings.coap.CoapEndpoint
import com.fitbit.goldengate.bindings.node.BluetoothAddressNodeKey
import com.fitbit.goldengate.node.NodeMapper
import com.nhaarman.mockitokotlin2.doReturn
import com.nhaarman.mockitokotlin2.mock
import com.nhaarman.mockitokotlin2.verify
import org.junit.Test
import kotlin.test.assertEquals

class CoapNodeProviderTest {

    private val bluetoothAddress = "bluetoothAddress"
    private val bluetoothAddressNodeKey = mock<BluetoothAddressNodeKey>()

    private val bluetoothAddressNodeKeyProvider = mock<(String) -> BluetoothAddressNodeKey> {
        on { invoke(bluetoothAddress) } doReturn bluetoothAddressNodeKey
    }

    private val stackNode = mock<StackNode<CoapEndpoint>>()

    private val stackNodeBuilder = mock<StackNodeBuilder<CoapEndpoint>> {
        on { build(bluetoothAddressNodeKey) } doReturn stackNode
    }

    private val nodeMapper = mock<NodeMapper> {
        on { get(bluetoothAddressNodeKey, stackNodeBuilder) } doReturn stackNode
    }

    @Test
    fun getNode() {
        val node = CoapNodeProvider(
                nodeMapper,
                stackNodeBuilder,
                bluetoothAddressNodeKeyProvider
        ).getNode(bluetoothAddress)

        assertEquals(stackNode, node)
        verify(nodeMapper).get(bluetoothAddressNodeKey, stackNodeBuilder)
        verify(bluetoothAddressNodeKeyProvider).invoke(bluetoothAddress)
    }

    @Test
    fun removeNode() {
        val node = CoapNodeProvider(
            nodeMapper,
            stackNodeBuilder,
            bluetoothAddressNodeKeyProvider
        ).removeNode(bluetoothAddress)
        verify(nodeMapper).removeNode(bluetoothAddressNodeKey)
    }
}

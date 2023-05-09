// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.node.stack

import com.fitbit.goldengate.bindings.coap.CoapEndpoint
import com.fitbit.goldengate.bindings.node.BluetoothAddressNodeKey
import com.fitbit.goldengate.node.NodeMapper
import org.mockito.kotlin.doReturn
import org.mockito.kotlin.mock
import org.mockito.kotlin.verify
import org.junit.Test
import kotlin.test.assertEquals

class CoapPeerProviderTest {

    private val bluetoothAddress = "bluetoothAddress"
    private val shouldSetStartMtu = { true }
    private val bluetoothAddressNodeKey = mock<BluetoothAddressNodeKey>()

    private val bluetoothAddressNodeKeyProvider = mock<(String) -> BluetoothAddressNodeKey> {
        on { invoke(bluetoothAddress) } doReturn bluetoothAddressNodeKey
    }

    private val stackNode = mock<StackPeer<CoapEndpoint>>()

    private val stackNodeBuilder = mock<StackPeerBuilder<CoapEndpoint>> {
        on { build(bluetoothAddressNodeKey) } doReturn stackNode
    }

    private val nodeMapper = mock<NodeMapper> {
        on { get(bluetoothAddressNodeKey, stackNodeBuilder) } doReturn stackNode
    }

    @Test
    fun getNode() {
        val node = CoapNodeProvider(
                shouldSetStartMtu,
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
            shouldSetStartMtu
            nodeMapper,
            stackNodeBuilder,
            bluetoothAddressNodeKeyProvider
        ).removeNode(bluetoothAddress)
        verify(nodeMapper).removeNode(bluetoothAddressNodeKey)
    }
}

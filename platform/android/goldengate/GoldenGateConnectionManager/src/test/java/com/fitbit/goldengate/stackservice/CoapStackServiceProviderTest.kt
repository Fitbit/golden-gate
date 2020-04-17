// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.stackservice

import com.fitbit.goldengate.bindings.coap.CoapEndpoint
import com.fitbit.goldengate.bindings.node.BluetoothAddressNodeKey
import com.fitbit.goldengate.node.stack.StackNode
import com.fitbit.goldengate.node.stack.StackNodeBuilder
import com.nhaarman.mockitokotlin2.doReturn
import com.nhaarman.mockitokotlin2.mock
import org.junit.Test

class CoapStackServiceProviderTest {

    @Test
    operator fun invoke() {
        val nodeKey = mock<BluetoothAddressNodeKey>()
        val coapEndpoint = mock<CoapEndpoint>()
        val stackNode = mock<StackNode<CoapEndpoint>> {
            on { stackService } doReturn coapEndpoint
        }
        val stackNodeBuilder = mock<StackNodeBuilder<CoapEndpoint>> {
            on { build(nodeKey) } doReturn stackNode
        }
        //This should not error out
        CoapStackServiceProvider.invoke(stackNodeBuilder = stackNodeBuilder).get(nodeKey)
    }
}

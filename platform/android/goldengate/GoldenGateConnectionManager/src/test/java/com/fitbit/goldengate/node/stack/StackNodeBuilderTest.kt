// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.node.stack

import com.fitbit.goldengate.bindings.DataSinkDataSource
import com.fitbit.goldengate.bindings.node.BluetoothAddressNodeKey
import com.fitbit.goldengate.bindings.stack.DtlsSocketNetifGattlink
import com.fitbit.goldengate.bindings.stack.GattlinkStackConfig
import com.fitbit.goldengate.bindings.stack.StackService
import com.nhaarman.mockitokotlin2.doReturn
import com.nhaarman.mockitokotlin2.mock
import org.junit.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertTrue

class StackNodeBuilderTest {

    private val stackServiceProvider = mock<() -> ThisStackService> {
        on { invoke() } doReturn ThisStackService()
    }
    private val stackNode = mock<StackNode<ThisStackService>> {
        on { stackService } doReturn ThisStackService()
        on { stackConfig } doReturn GattlinkStackConfig
    }

    val key = mock<BluetoothAddressNodeKey>()

    private val buildStackNode = mock<(BluetoothAddressNodeKey) -> StackNode<ThisStackService>> {
        on { invoke(key) } doReturn stackNode
    }
    private val stackNodeBuilder = StackNodeBuilder(
            ThisStackService::class.java,
            GattlinkStackConfig,
            buildStackNode
    )

    @Test
    fun defaultConstructor() {
        StackNodeBuilder(
                ThisStackService::class.java,
                stackServiceProvider,
                GattlinkStackConfig
        )
    }

    @Test
    fun build() {
        assertEquals(stackNode, stackNodeBuilder.build(key))
    }

    @Test
    fun doesBuild() {
        val otherNode = mock<StackNode<ThisStackService>> {
            on { stackService } doReturn ThisStackService()
            on { stackConfig } doReturn GattlinkStackConfig
        }
        assertTrue(stackNodeBuilder.doesBuild(otherNode))
    }

    @Test
    fun doesBuildNotBuildWhenNotStackNode() {
        assertFalse(stackNodeBuilder.doesBuild(mock()))
    }

    @Test
    fun doesBuildNotBuildWhenNotSameConfig() {
        val otherNode = mock<StackNode<ThisStackService>> {
            on { stackService } doReturn ThisStackService()
            on { stackConfig } doReturn DtlsSocketNetifGattlink()
        }
        assertFalse(stackNodeBuilder.doesBuild(otherNode))
    }

    @Test
    fun doesBuildNotBuildWhenNotSameService() {
        val otherNode = mock<StackNode<OtherStackService>> {
            on { stackService } doReturn OtherStackService()
            on { stackConfig } doReturn GattlinkStackConfig
        }
        assertFalse(stackNodeBuilder.doesBuild(otherNode))
    }

    class ThisStackService: StackService {
        override fun attach(dataSinkDataSource: DataSinkDataSource) {}

        override fun detach() {}

        override fun close() {}

    }

    class OtherStackService: StackService {
        override fun attach(dataSinkDataSource: DataSinkDataSource) {}

        override fun detach() {}

        override fun close() {}
    }
}

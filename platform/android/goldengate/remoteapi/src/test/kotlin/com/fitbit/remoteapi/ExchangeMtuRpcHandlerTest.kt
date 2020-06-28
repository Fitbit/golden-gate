// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.remoteapi

import android.content.Context
import co.nstant.`in`.cbor.CborBuilder
import co.nstant.`in`.cbor.model.Array
import co.nstant.`in`.cbor.model.Map
import co.nstant.`in`.cbor.model.SimpleValue
import co.nstant.`in`.cbor.model.UnicodeString
import co.nstant.`in`.cbor.model.UnsignedInteger
import com.fitbit.goldengate.node.stack.StackPeer
import com.fitbit.remoteapi.handlers.ExchangeMtuRpc
import com.nhaarman.mockitokotlin2.doReturn
import com.nhaarman.mockitokotlin2.mock
import com.nhaarman.mockitokotlin2.verify
import org.junit.Test
import org.junit.runner.RunWith
import org.junit.runners.JUnit4
import kotlin.test.assertEquals

@RunWith(JUnit4::class)
class ExchangeMtuRpcHandlerTest {

    private val mtuParam = "mtu"
    private val context = mock<Context>()

    @Test
    fun testCommandParsing() {
        // { "mtu" : 1234 }
        val mtu = 1234
        val dataItems = CborBuilder()
                .addMap()
                .put(mtuParam, mtu.toLong())
                .end()
                .build()
        assertEquals(mtu, ExchangeMtuRpc.Parser().parse(dataItems))
    }

    @Test
    fun testProcessor() {
        val mtu = 1234
        val node = mock<StackPeer<*>> {
        }
        val remoteApiConfigurationState = mock<RemoteApiConfigurationState> {
            on { getNode(context, null) } doReturn node
        }
        assertEquals(mtu, ExchangeMtuRpc.Processor(context, remoteApiConfigurationState).process(mtu))
        verify(node).requestMtu(mtu)
    }

    @Test(expected = IllegalArgumentException::class)
    fun testThrowsOnInvalidParamKeyAndValueType() {
        // { "invalid" : "? }
        val dataItems = CborBuilder().addMap().put("invalid", "?").end().build()
        ExchangeMtuRpc.Parser().parse(dataItems)
    }

    @Test(expected = IllegalArgumentException::class)
    fun testThrowsOnNullValue() {
        // { "mtu" : null }
        val dataItems = listOf(Map().put(UnicodeString(mtuParam), SimpleValue.NULL))
        ExchangeMtuRpc.Parser().parse(dataItems)
    }

    @Test(expected = IllegalArgumentException::class)
    fun testThrowsOnInvalidValueType() {
        // { "mtu" : [2] }
        val dataItems = listOf(Map().put(UnicodeString(mtuParam), Array().add(UnsignedInteger(2))))
        ExchangeMtuRpc.Parser().parse(dataItems)
    }
}

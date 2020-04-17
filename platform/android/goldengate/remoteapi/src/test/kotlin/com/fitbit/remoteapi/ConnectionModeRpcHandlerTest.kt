// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.remoteapi

import co.nstant.`in`.cbor.CborBuilder
import co.nstant.`in`.cbor.model.Array
import co.nstant.`in`.cbor.model.Map
import co.nstant.`in`.cbor.model.UnicodeString
import co.nstant.`in`.cbor.model.UnsignedInteger
import com.fitbit.remoteapi.handlers.ConnectionModeRpc
import org.junit.Test
import org.junit.runner.RunWith
import org.junit.runners.JUnit4
import kotlin.test.assertEquals

@RunWith(JUnit4::class)
class ConnectionModeRpcHandlerTest {

    private val nameParam = "peer"
    private val connectionModeParam = "speed"

    @Test
    fun testCommandParsing() {
        // { "name" : "this is my name", , "speed" : "fast"  }
        val nameString = "this is my name"
        val speedString = "fast"
        val dataItems = CborBuilder().addMap()
            .put(nameParam, nameString)
            .put(connectionModeParam, speedString)
            .end().build()
        assertEquals(speedString, ConnectionModeRpc.Parser().parse(dataItems))
    }

    @Test(expected = IllegalArgumentException::class)
    fun testThrowsOnInvalidParamKeyAndValueType() {
        // { "invalid" : 2 }
        val dataItems = CborBuilder().addMap().put("invalid", 2).end().build()
        ConnectionModeRpc.Parser().parse(dataItems)
    }

    @Test(expected = IllegalArgumentException::class)
    fun testThrowsOnInvalidConnectionValueType() {
        // { "name" : "this is my name", "speed" : [2] }
        val nameString = "this is my name"

        val dataItems = listOf(Map()
            .put(UnicodeString(nameParam), UnicodeString(nameString))
            .put(UnicodeString(connectionModeParam), Array().add(UnsignedInteger(2))))
        ConnectionModeRpc.Parser().parse(dataItems)
    }
}

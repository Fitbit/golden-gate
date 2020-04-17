// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bindings.sockets.bsd

import com.fitbit.goldengate.bindings.BaseTest
import com.fitbit.goldengate.bindings.coap.COAP_DEFAULT_PORT
import org.junit.After
import org.junit.Assert.assertTrue
import org.junit.Test
import java.net.Inet4Address
import java.net.Inet6Address
import java.net.InetSocketAddress

class BsdDatagramSocketTest : BaseTest() {

    private var socket: BsdDatagramSocket? = null

    @After
    fun tearDown() {
        socket?.close()
    }

    @Test(expected = IllegalArgumentException::class)
    fun shouldThrowExceptionIfInet6RemoteIpAddressIsProvided() {
        BsdDatagramSocket(BsdDatagramSocketAddress(
            localPort = COAP_DEFAULT_PORT,
            remoteAddress = InetSocketAddress(
                Inet6Address.getByName("1080:0:0:0:8:800:200C:417A"),
                COAP_DEFAULT_PORT
            )
        ))
    }

    @Test
    fun shouldCreateSocketInServerOnlyMode() {
        // no remote address is specified, this socket can only receive request from remote client
        socket = BsdDatagramSocket(BsdDatagramSocketAddress(localPort = COAP_DEFAULT_PORT))
        assertTrue(socket!!.thisPointer > 0)
    }

    @Test
    fun shouldCreateSocketInClientAndServerMode() {
        // This socket can receive request from any client, and can only send request to specified remote address
        socket = BsdDatagramSocket(BsdDatagramSocketAddress(
            localPort = COAP_DEFAULT_PORT,
            remoteAddress = InetSocketAddress(
                Inet4Address.getByName("127.0.0.1"),
                COAP_DEFAULT_PORT
            )
        ))
        assertTrue(socket!!.thisPointer > 0)
    }

    @Test
    fun shouldReturnReferenceToDataSink() {
        socket = BsdDatagramSocket(BsdDatagramSocketAddress(localPort = COAP_DEFAULT_PORT))
        assertTrue(socket!!.getAsDataSinkPointer() > 0)
    }

    @Test
    fun shouldReturnReferenceToDataSource() {
        socket = BsdDatagramSocket(BsdDatagramSocketAddress(localPort = COAP_DEFAULT_PORT))
        assertTrue(socket!!.getAsDataSourcePointer() > 0)
    }

}

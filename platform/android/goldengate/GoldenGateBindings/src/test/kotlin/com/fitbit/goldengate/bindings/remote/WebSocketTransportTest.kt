// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bindings.remote

import com.fitbit.goldengate.bindings.BaseTest
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Test
import java.net.ServerSocket

class WebSocketTransportTest: BaseTest() {

    @Test
    fun testWebsocketFailsWithInvalidAddress() {
        val webSocketTransport = WebSocketTransport("ws://0.0.0.0")
        assertEquals(0, webSocketTransport.receive().size)
    }

    @Test
    fun testConnectsToLocalhost() {
        val server = ServerSocket(30209)
        server.soTimeout = 200 //accept will give up after this many milliseconds.
        val webSocketTransport = WebSocketTransport("ws://127.0.0.1:30209")
        val socket = server.accept()
        //If we reach here without an exception being thrown, the connection was successful
        assertNotNull(webSocketTransport)
        assertNotNull(socket)
    }

    /*
    Not sure what the best way to test the WebSocketTransport without having a dependency on a
    local or remote webserver. Using the java classes like in the above test means the websocket upgrade
    request ends up being the first message through the websocket.
     */
}

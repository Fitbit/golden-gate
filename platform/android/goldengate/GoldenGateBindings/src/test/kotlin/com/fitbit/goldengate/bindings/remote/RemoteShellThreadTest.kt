// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bindings.remote

import com.fitbit.goldengate.bindings.BaseTest
import java.util.Base64
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit
import okhttp3.WebSocket
import okio.ByteString.Companion.toByteString
import org.junit.Assert
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.mockito.kotlin.doReturn
import org.mockito.kotlin.mock

class RemoteShellThreadTest : BaseTest() {

  private val callTestNameBytes = Base64.getDecoder().decode("omZtZXRob2RodGVzdE5hbWViaWQB")
  private val callTestName =
    callTestNameBytes.toByteString(0, callTestNameBytes.size) // {"method":"testName", "id":1}

  private val mockWebSocket: WebSocket = mock()

  @Test
  fun testRemoteShellInit() {
    val webSocketTransport = WebSocketTransport("http://0.0.0.0")
    val shellThread = RemoteShellThread(webSocketTransport)
    Assert.assertNotEquals("RemoteShell pointer was null!", 0, shellThread.thisPointer)
  }

  @Test
  fun testRegisterHandler() {
    val webSocketTransport = WebSocketTransport("http://0.0.0.0", true)
    val shellThread = RemoteShellThread(webSocketTransport)
    val handler =
      object : CborHandler {
        override val name = "testName"

        override fun handle(requestParamsCbor: ByteArray): ByteArray {
          // Do nothing
          return ByteArray(0)
        }
      }
    shellThread.registerHandler(handler)
    Assert.assertEquals("Handler set size not 1", 1, shellThread.getHandlers().size)

    Assert.assertNotEquals("Handler pointer is null", 0, shellThread.getHandlers().first())
  }

  @Test
  fun testHandleMessageIsCalled() {
    val webSocketTransport = WebSocketTransport("http://0.0.0.0", true)
    val shellThread = RemoteShellThread(webSocketTransport)
    val latch = CountDownLatch(1)
    val handler =
      object : CborHandler {
        override val name = "testName"

        override fun handle(requestParamsCbor: ByteArray): ByteArray {
          latch.countDown()
          return ByteArray(0)
        }
      }
    shellThread.registerHandler(handler)
    shellThread.start()
    webSocketTransport.onMessage(mockWebSocket, callTestName)
    assertTrue("Timed out waiting for handler to be called", latch.await(50, TimeUnit.MILLISECONDS))
    webSocketTransport.onClosed(mockWebSocket, 0, "")
  }

  @Test
  fun testHandleMessageIsNotCalledForNameMismatch() {
    val webSocketTransport = WebSocketTransport("http://0.0.0.0", true)
    val shellThread = RemoteShellThread(webSocketTransport)
    val latch = CountDownLatch(1)
    val handler =
      object : CborHandler {
        override val name = "WRONGNAME"

        override fun handle(requestParamsCbor: ByteArray): ByteArray {
          latch.countDown()
          return ByteArray(0)
        }
      }
    shellThread.registerHandler(handler)
    shellThread.start()
    webSocketTransport.onMessage(mockWebSocket, callTestName)
    assertFalse(
      "Handler was called despite a name mismatch",
      latch.await(50, TimeUnit.MILLISECONDS)
    )
    webSocketTransport.onClosed(mockWebSocket, 0, "")
  }

  @Test
  fun testHandlerIsCleanedUpAfterClose() {
    val webSocketTransport = WebSocketTransport("http://0.0.0.0")
    val shellThread = RemoteShellThread(webSocketTransport)
    val handler = mock<CborHandler> { on { name } doReturn "" }
    shellThread.registerHandler(handler)
    Assert.assertEquals("Handler set size not 1", 1, shellThread.getHandlers().size)
    shellThread.start()
    webSocketTransport.onClosed(mockWebSocket, 0, "")
    val waitForThreadToDieTimeout = 300L
    shellThread.join(waitForThreadToDieTimeout)
    assertFalse(
      "Shell thread did not die within $waitForThreadToDieTimeout ms",
      shellThread.isAlive
    )
    assertEquals("Handlers collection not empty", 0, shellThread.getHandlers().size)
  }
}

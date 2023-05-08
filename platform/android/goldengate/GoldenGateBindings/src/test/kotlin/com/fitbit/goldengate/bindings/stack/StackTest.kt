// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bindings.stack

import com.fitbit.goldengate.bindings.BaseTest
import com.fitbit.goldengate.bindings.coap.CoapEndpoint
import com.fitbit.goldengate.bindings.dtls.DtlsProtocolStatus
import com.fitbit.goldengate.bindings.io.Blaster
import com.fitbit.goldengate.bindings.io.RxSource
import com.fitbit.goldengate.bindings.io.TxSink
import com.fitbit.goldengate.bindings.node.NodeKey
import kotlin.test.assertFailsWith
import org.junit.After
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.mockito.kotlin.doReturn
import org.mockito.kotlin.mock

private const val ONE_MINUTE_IN_MILLISECONDS: Int = 60_000

class StackTest : BaseTest() {

  private val mockNodeKey: NodeKey<String> = mock { on { value } doReturn "node_key_1" }

  private lateinit var txSink: TxSink
  private lateinit var rxSource: RxSource

  @Before
  fun setup() {
    txSink = TxSink()
    rxSource = RxSource()
  }

  @After
  fun tearDown() {
    txSink.close()
    rxSource.close()
  }

  @Test
  fun shouldCreateStack() {
    Stack(mockNodeKey, txSink.thisPointer, rxSource.thisPointer).use { stack ->
      assertTrue(stack.thisPointer > 0)
    }
  }

  @Test
  fun canCanStartStack() {
    Stack(mockNodeKey, txSink.thisPointer, rxSource.thisPointer).use { stack -> stack.start() }
  }

  @Test
  fun canStartMultipleTimes() {
    Stack(mockNodeKey, txSink.thisPointer, rxSource.thisPointer).use { stack ->
      stack.start()
      stack.start()
    }
  }

  @Test
  fun canCloseMultipleTimes() {
    Stack(mockNodeKey, txSink.thisPointer, rxSource.thisPointer).use { stack ->
      stack.start()
      stack.close()
      stack.close() // This should not cause a double-free
    }
  }

  @Test
  fun getAsDataSinkPointerThrowsAfterClose() {
    Stack(mockNodeKey, txSink.thisPointer, rxSource.thisPointer).use { stack ->
      stack.start()
      stack.close()
      assertFailsWith(IllegalStateException::class) { stack.getAsDataSinkPointer() }
      Unit // Don't return anything from the use block
    }
  }

  @Test
  fun getAsDataSourcePointerThrowsAfterClose() {
    Stack(mockNodeKey, txSink.thisPointer, rxSource.thisPointer).use { stack ->
      stack.start()
      stack.close()
      assertFailsWith(IllegalStateException::class) { stack.getAsDataSourcePointer() }
      Unit // Don't return anything from the use block
    }
  }

  @Test
  fun updateMtuReturnsFalseAfterClose() {
    Stack(mockNodeKey, txSink.thisPointer, rxSource.thisPointer).use { stack ->
      stack.start()
      stack.close()
      assertFalse(stack.updateMtu(30))
    }
  }

  @Test
  fun canAttachAndDetachCoapEndpoint() {
    val coapEndpoint = CoapEndpoint()

    Stack(mockNodeKey, txSink.thisPointer, rxSource.thisPointer).use { stack ->
      coapEndpoint.attach(stack)
      assertNotNull("Coap endpoint not attached", coapEndpoint.dataSinkDataSource)
      stack.start()
      coapEndpoint.detach()
      assertNull("Coap endpoint not detached", coapEndpoint.dataSinkDataSource)
      stack.close()
    }
  }

  @Test
  fun canAttachBlaster() {
    Stack(mockNodeKey, txSink.thisPointer, rxSource.thisPointer, SocketNetifGattlink()).use {
      val blaster = Blaster(true)
      blaster.close()
    }
  }

  @Test
  fun shouldNotEmitTlsStateChangeIfStackNotStarted() {
    Stack(mockNodeKey, txSink.thisPointer, rxSource.thisPointer).use { stack ->
      stack.dtlsEventObservable.test().assertNoValues()
    }
  }

  @Test
  fun shouldEmitTlsStateChangeToHandshakeOnStackStart() {
    Stack(mockNodeKey, txSink.thisPointer, rxSource.thisPointer).use { stack ->
      stack.start()
      stack.dtlsEventObservable.test().assertValue {
        it.state == DtlsProtocolStatus.TlsProtocolState.TLS_STATE_HANDSHAKE
      }
    }
  }

  @Test
  fun shouldEmitTlsStateChangeToSession() {
    Stack(mockNodeKey, txSink.thisPointer, rxSource.thisPointer).use { stack ->
      stack.onDtlsStatusChange(2, -1, "BOOTSTRAP".toByteArray())
      stack.dtlsEventObservable.test().assertValue {
        it.state == DtlsProtocolStatus.TlsProtocolState.TLS_STATE_SESSION
      }
    }
  }

  @Test
  fun shouldEmitUnknownTlsStateChange() {
    Stack(mockNodeKey, txSink.thisPointer, rxSource.thisPointer).use { stack ->
      stack.onDtlsStatusChange(999, -1, "BOOTSTRAP".toByteArray())
      stack.dtlsEventObservable.test().assertValue {
        it.state == DtlsProtocolStatus.TlsProtocolState.TLS_STATE_UNKNOWN
      }
    }
  }

  @Test
  fun shouldEmitStackEvent() {
    Stack(mockNodeKey, txSink.thisPointer, rxSource.thisPointer).use { stack ->
      stack.onStackEvent(StackEvent.GattlinkBufferOverThreshold.eventId, 0)
      stack.stackEventObservable.test().assertValue { it == StackEvent.GattlinkBufferOverThreshold }
    }
  }

  @Test
  fun shouldEmitStackStalledEvent() {
    Stack(mockNodeKey, txSink.thisPointer, rxSource.thisPointer).use { stack ->
      stack.onStackEvent(
        StackEvent.GattlinkSessionStall(ONE_MINUTE_IN_MILLISECONDS).eventId,
        ONE_MINUTE_IN_MILLISECONDS
      )
      stack.stackEventObservable.test().assertValue {
        it is StackEvent.GattlinkSessionStall && it.stalledTime == ONE_MINUTE_IN_MILLISECONDS
      }
    }
  }

  @Test
  fun canCallMtuChangeOnStack() {
    Stack(mockNodeKey, txSink.thisPointer, rxSource.thisPointer).use { stack ->
      assertTrue(stack.updateMtu(182))
    }
  }
}

package com.fitbit.goldengate.bindings.services

import com.fitbit.goldengate.bindings.BaseTest
import com.fitbit.goldengate.bindings.coap.CoapEndpoint
import com.fitbit.goldengate.bindings.io.RxSource
import com.fitbit.goldengate.bindings.io.TxSink
import com.fitbit.goldengate.bindings.node.BluetoothAddressNodeKey
import com.fitbit.goldengate.bindings.remote.RemoteShellThread
import com.fitbit.goldengate.bindings.remote.WebSocketTransport
import com.fitbit.goldengate.bindings.stack.Stack
import org.junit.Test

class CoapTestServiceTest: BaseTest() {

    @Test
    fun testSetupStackWithCoapServices() {
        // Given
        val webSocketTransport = WebSocketTransport("http://0.0.0.0")
        val remoteShellThread = RemoteShellThread(webSocketTransport)
        val endpoint = CoapEndpoint()
        val coapServices = CoapServices.Provider(remoteShellThread).get(endpoint)
        val txSink = TxSink()
        val rxSource = RxSource()
        val stack = Stack(
            BluetoothAddressNodeKey("AA:BB:CC:DD:EE:FF"),
            txSink.thisPointer,
            rxSource.thisPointer
        )

        // When
        coapServices.attach(stack)
        coapServices.detach()
        coapServices.close()

        // If we get here we didn't crash and the test passed.
    }

}

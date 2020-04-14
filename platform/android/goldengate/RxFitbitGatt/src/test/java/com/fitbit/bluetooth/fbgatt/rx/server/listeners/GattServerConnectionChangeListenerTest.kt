package com.fitbit.bluetooth.fbgatt.rx.server.listeners

import android.bluetooth.BluetoothDevice
import com.fitbit.bluetooth.fbgatt.GattServerConnection
import com.fitbit.bluetooth.fbgatt.TransactionResult
import com.fitbit.bluetooth.fbgatt.TransactionResult.TransactionResultStatus
import com.fitbit.bluetooth.fbgatt.rx.PeripheralConnectionStatus
import com.fitbit.bluetooth.fbgatt.rx.server.GattServerResponseSender
import com.nhaarman.mockitokotlin2.any
import com.nhaarman.mockitokotlin2.anyOrNull
import com.nhaarman.mockitokotlin2.doReturn
import com.nhaarman.mockitokotlin2.mock
import com.nhaarman.mockitokotlin2.whenever
import io.reactivex.Completable
import org.junit.Before
import org.junit.Test
import java.util.UUID

class GattServerConnectionChangeListenerTest {

    private val device1 = mock<BluetoothDevice> {
        on { address } doReturn "1"
    }

    private val mockGattServerConnection = mock<GattServerConnection>()
    private val mockGattServerResponseSender = mock<GattServerResponseSender>()

    private val listener = GattServerConnectionChangeListener

    @Before
    fun setup() {
        mockGattServerResponse()
    }

    @Test
    fun shouldReceiveConnectedStatusForSuccessResponse() {
        listener.onServerConnectionStateChanged(
                device = device1,
                connection = mockGattServerConnection,
                result = mockTransactionResult()
        )

        listener.getDataObservable(device1)
                .test()
                .assertValue { it == PeripheralConnectionStatus.CONNECTED }
    }

    @Test
    fun shouldReceiveDisconnectedStatusForNonSuccessResponse() {
        TransactionResultStatus.values().forEach { status ->
            if (status != TransactionResultStatus.SUCCESS) {
                listener.onServerConnectionStateChanged(
                        device = device1,
                        connection = mockGattServerConnection,
                        result = mockTransactionResult(connectedStatus = status)
                )

                listener.getDataObservable(device1)
                        .test()
                        .assertValue { it == PeripheralConnectionStatus.DISCONNECTED }

            }
        }
    }

    private fun mockGattServerResponse() {
        whenever(mockGattServerResponseSender.send(any(), any(), any(), any(), any(), anyOrNull()))
                .thenReturn(Completable.complete())
    }

    private fun mockTransactionResult(
            serviceId: UUID = UUID.fromString("FFFFFFFF-FFFF-FFFF-FFFF-FFFFFFFFFFFF"),
            responseNeeded: Boolean = true,
            connectedStatus: TransactionResultStatus = TransactionResultStatus.SUCCESS
    ): TransactionResult {
        return mock {
            on { requestId } doReturn 1
            on { serviceUuid } doReturn serviceId
            on { isResponseRequired } doReturn responseNeeded
            on { resultStatus } doReturn connectedStatus
        }

    }

}

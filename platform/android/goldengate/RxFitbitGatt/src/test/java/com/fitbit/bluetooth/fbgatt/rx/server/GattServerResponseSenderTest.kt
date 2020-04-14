package com.fitbit.bluetooth.fbgatt.rx.server

import android.bluetooth.BluetoothGatt
import com.fitbit.bluetooth.fbgatt.FitbitBluetoothDevice
import com.fitbit.bluetooth.fbgatt.TransactionResult.TransactionResultStatus
import com.fitbit.bluetooth.fbgatt.rx.GattTransactionException
import com.fitbit.bluetooth.fbgatt.rx.mockGattServerConnection
import com.fitbit.bluetooth.fbgatt.rx.mockGattTransactionCompletion
import com.fitbit.bluetooth.fbgatt.tx.SendGattServerResponseTransaction
import com.nhaarman.mockitokotlin2.any
import com.nhaarman.mockitokotlin2.anyOrNull
import com.nhaarman.mockitokotlin2.doReturn
import com.nhaarman.mockitokotlin2.mock
import org.junit.Test

class GattServerResponseSenderTest {

    private val mockFitbitBluetoothDevice = mock<FitbitBluetoothDevice>()

    private val mockTransaction = mock<SendGattServerResponseTransaction>()
    private val mockTransactionProvider = mock<SendGattServerResponseTransactionProvider> {
        on { provide(any(), any(), any(), any(), any(), any(), anyOrNull()) } doReturn mockTransaction
    }

    private val sender = GattServerResponseSender(
            mockGattServerConnection,
            mockTransactionProvider
    )

    @Test
    fun shouldSendResponseIfTransactionSucceeds() {
        mockTransaction.mockGattTransactionCompletion(TransactionResultStatus.SUCCESS)

        sender.send(
                device = mockFitbitBluetoothDevice,
                requestId = 1,
                status = BluetoothGatt.GATT_SUCCESS,
                offset = 0
        )
                .test()
                .assertComplete()
    }

    @Test
    fun shouldFailToSendResponseIfTransactionFails() {
        mockTransaction.mockGattTransactionCompletion(TransactionResultStatus.FAILURE)

        sender.send(
                device = mockFitbitBluetoothDevice,
                requestId = 1,
                status = BluetoothGatt.GATT_SUCCESS,
                offset = 0
        )
                .test()
                .assertError { it is GattTransactionException }
    }
}

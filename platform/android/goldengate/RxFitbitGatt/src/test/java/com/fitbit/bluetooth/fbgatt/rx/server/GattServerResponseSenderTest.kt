// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.bluetooth.fbgatt.rx.server

import android.bluetooth.BluetoothGatt
import com.fitbit.bluetooth.fbgatt.FitbitBluetoothDevice
import com.fitbit.bluetooth.fbgatt.TransactionResult.TransactionResultStatus
import com.fitbit.bluetooth.fbgatt.rx.GattTransactionException
import com.fitbit.bluetooth.fbgatt.rx.mockGattServerConnection
import com.fitbit.bluetooth.fbgatt.rx.mockGattTransactionCompletion
import com.fitbit.bluetooth.fbgatt.tx.SendGattServerResponseTransaction
import org.mockito.kotlin.any
import org.mockito.kotlin.anyOrNull
import org.mockito.kotlin.doReturn
import org.mockito.kotlin.mock
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

// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.bluetooth.fbgatt.rx.server

import android.bluetooth.BluetoothGattService
import com.fitbit.bluetooth.fbgatt.TransactionResult.TransactionResultStatus.FAILURE
import com.fitbit.bluetooth.fbgatt.TransactionResult.TransactionResultStatus.SUCCESS
import com.fitbit.bluetooth.fbgatt.rx.GattTransactionException
import com.fitbit.bluetooth.fbgatt.rx.mockGattServerConnection
import com.fitbit.bluetooth.fbgatt.rx.mockGattTransactionCompletion
import com.fitbit.bluetooth.fbgatt.rx.mockTransactionResult
import com.fitbit.bluetooth.fbgatt.tx.GetGattServerServicesTransaction
import com.nhaarman.mockitokotlin2.any
import com.nhaarman.mockitokotlin2.doReturn
import com.nhaarman.mockitokotlin2.mock
import com.nhaarman.mockitokotlin2.whenever
import org.junit.Test

class GetGattServerServicesTest {

    private val mockTransaction = mock<GetGattServerServicesTransaction>()
    private val mockTransactionProvider = mock<GetGattServerServicesTransactionProvider> {
        on { provide(any(), any(), any()) } doReturn mockTransaction
    }

    private val getServices = GetGattServerServices(
        mockGattServerConnection,
        mockTransactionProvider
    )

    @Test
    fun shouldReturnGattServicesIfTransactionSucceeds() {
        val services = listOf(mock<BluetoothGattService>())
        mockTransaction.mockGattTransactionCompletion(SUCCESS)
        whenever(mockTransactionResult.services).thenReturn(services)

        getServices.get()
            .test()
            .assertValue { it == services }
    }

    @Test
    fun shouldFailIfTransactionFails() {
        mockTransaction.mockGattTransactionCompletion(FAILURE)
        getServices.get()
            .test()
            .assertError { it is GattTransactionException }
    }
}

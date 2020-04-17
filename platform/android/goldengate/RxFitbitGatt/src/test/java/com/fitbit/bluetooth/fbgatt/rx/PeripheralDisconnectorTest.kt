// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.bluetooth.fbgatt.rx

import android.bluetooth.BluetoothGatt
import com.fitbit.bluetooth.fbgatt.GattTransaction
import com.fitbit.bluetooth.fbgatt.TransactionResult
import com.fitbit.bluetooth.fbgatt.rx.client.ClientTransactionProvider
import com.nhaarman.mockitokotlin2.any
import com.nhaarman.mockitokotlin2.doReturn
import com.nhaarman.mockitokotlin2.mock
import io.reactivex.observers.TestObserver
import org.junit.Test

class PeripheralDisconnectorTest {

    private val mockTransaction = mock<GattTransaction>()
    private val mockClientTransactionProvider: ClientTransactionProvider = mock {
        on { getDisconnectTransactionFor(any()) } doReturn mockTransaction
    }

    private val disconnector = PeripheralDisconnector(
        mockFitbitGatt,
        mockClientTransactionProvider
    )

    @Test
    fun testDisconnectSuccess() {
        // Given
        mockGattConnection.mockConnected(true)
        mockTransaction.mockGattTransactionCompletion(TransactionResult.TransactionResultStatus.SUCCESS)

        // When
        val testObserver = TestObserver.create<BluetoothGatt>()
        disconnector.disconnect(mockGattConnection).subscribe(testObserver)

        // Then
        testObserver.assertComplete()
    }

    @Test
    fun testDisconnectWhenNotConnected() {
        // Given
        mockGattConnection.mockConnected(false)
        mockTransaction.mockGattTransactionCompletion(TransactionResult.TransactionResultStatus.SUCCESS)

        // When
        val testObserver = TestObserver.create<BluetoothGatt>()
        disconnector.disconnect(mockGattConnection).subscribe(testObserver)

        // Then
        testObserver.assertComplete()
    }

    @Test
    fun testDisconnectFailure() {
        // Given
        mockGattConnection.mockConnected(true)
        mockTransaction.mockGattTransactionCompletion(TransactionResult.TransactionResultStatus.FAILURE)

        // When
        val testObserver = TestObserver.create<BluetoothGatt>()
        disconnector.disconnect(mockGattConnection).subscribe(testObserver)

        // Then
        testObserver.assertError { it is GattTransactionException }
    }

    @Test
    fun shouldDisconnectIfDeviceIsKnown() {
        mockFitbitGatt.mockDeviceKnown(true)
        mockGattConnection.mockConnected(true)
        mockTransaction.mockGattTransactionCompletion(TransactionResult.TransactionResultStatus.SUCCESS)

        disconnector.disconnect(mockBluetoothAddress)
            .test()
            .assertComplete()
    }

    @Test
    fun shouldErrorIfDeviceIsNotKnown() {
        mockFitbitGatt.mockDeviceKnown(false)
        mockGattConnection.mockConnected(true)
        mockTransaction.mockGattTransactionCompletion(TransactionResult.TransactionResultStatus.SUCCESS)

        disconnector.disconnect(mockBluetoothAddress)
            .test()
            .assertError { it is NoSuchElementException }
    }

}

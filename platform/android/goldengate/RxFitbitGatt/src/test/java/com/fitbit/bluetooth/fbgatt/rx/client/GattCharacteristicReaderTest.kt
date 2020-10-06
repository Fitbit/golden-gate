// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.bluetooth.fbgatt.rx.client

import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattService
import com.fitbit.bluetooth.fbgatt.GattClientTransaction
import com.fitbit.bluetooth.fbgatt.GattState
import com.fitbit.bluetooth.fbgatt.GattTransaction
import com.fitbit.bluetooth.fbgatt.TransactionResult
import com.fitbit.bluetooth.fbgatt.rx.GattCharacteristicException
import com.fitbit.bluetooth.fbgatt.rx.GattServiceNotFoundException
import com.fitbit.bluetooth.fbgatt.rx.GattTransactionException
import com.fitbit.bluetooth.fbgatt.rx.mockGattConnection
import com.fitbit.bluetooth.fbgatt.rx.mockGattTransactionCompletion
import com.nhaarman.mockitokotlin2.any
import com.nhaarman.mockitokotlin2.doReturn
import com.nhaarman.mockitokotlin2.mock
import com.nhaarman.mockitokotlin2.whenever
import io.reactivex.Completable
import io.reactivex.Single
import io.reactivex.schedulers.Schedulers
import org.junit.Test
import java.util.Arrays
import java.util.UUID

class GattCharacteristicReaderTest {

    private val mockServiceId = UUID.fromString("FFFFFFFF-FFFF-FFFF-FFFF-FFFFFFFFFFF1")
    private val mockCharacteristicId = UUID.fromString("FFFFFFFF-FFFF-FFFF-FFFF-FFFFFFFFFFF2")

    private val mockBluetoothGattService = mock<BluetoothGattService>()
    private val mockBluetoothGattCharacteristic = mock<BluetoothGattCharacteristic>()
    private val mockGattTransaction: GattClientTransaction = mock()
    private val mockTransactionProvider: ReadGattCharacteristicTransactionProvider = mock {
        on { provide(any(), any()) } doReturn Single.just(mockGattTransaction)
    }
    private val mockGattClientConnectionStateSetter = mock<GattClientConnectionStateSetter> {
        on { set(mockGattConnection, GattState.IDLE) } doReturn Completable.complete()
    }

    private val reader = GattCharacteristicReader(
        gattConnection = mockGattConnection,
        readTransactionProvider = mockTransactionProvider,
        gattClientConnectionStateSetter = mockGattClientConnectionStateSetter,
        scheduler = Schedulers.trampoline()
    )

    @Test
    fun shouldReadIfGattTransactionSucceeds() {
        mockGattServiceFound()
        mockGattCharacteristicFound()
        mockTransactionSuccess()

        val expectedData = byteArrayOf(0x01)
        whenever(mockBluetoothGattCharacteristic.value).thenReturn(expectedData)

        reader.read(mockServiceId, mockCharacteristicId)
            .test()
            .assertValue { Arrays.equals(expectedData, it)}
    }

    @Test
    fun shouldFailToReadIfGattServiceNotFound() {
        mockGattServiceNotFound()
        mockGattCharacteristicFound()
        mockTransactionSuccess()

        reader.read(mockServiceId, mockCharacteristicId)
            .test()
            .assertError { it is GattServiceNotFoundException }
    }

    @Test
    fun shouldFailToReadIfGattCharacteristicNotFound() {
        mockGattServiceFound()
        mockGattCharacteristicNotFound()
        mockTransactionSuccess()

        reader.read(mockServiceId, mockCharacteristicId)
            .test()
            .assertError { it is GattCharacteristicException }
    }

    @Test
    fun shouldFailToReadIfGattTransactionFails() {
        mockGattServiceFound()
        mockGattCharacteristicFound()
        mockTransactionFailure()

        reader.read(mockServiceId, mockCharacteristicId)
            .test()
            .assertError { it is GattTransactionException }
    }

    private fun mockGattServiceFound() =
        whenever(mockGattConnection.getRemoteGattService(mockServiceId)).thenReturn(mockBluetoothGattService)

    private fun mockGattServiceNotFound() =
        whenever(mockGattConnection.getRemoteGattService(mockServiceId)).thenReturn(null)

    private fun mockGattCharacteristicFound() =
        whenever(mockBluetoothGattService.getCharacteristic(mockCharacteristicId)).thenReturn(mockBluetoothGattCharacteristic)

    private fun mockGattCharacteristicNotFound() =
        whenever(mockBluetoothGattService.getCharacteristic(mockCharacteristicId)).thenReturn(null)

    private fun mockTransactionSuccess() =
        mockGattTransaction.mockGattTransactionCompletion(TransactionResult.TransactionResultStatus.SUCCESS)

    private fun mockTransactionFailure() =
        mockGattTransaction.mockGattTransactionCompletion(TransactionResult.TransactionResultStatus.FAILURE)

}

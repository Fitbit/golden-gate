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
import org.mockito.kotlin.any
import org.mockito.kotlin.doReturn
import org.mockito.kotlin.mock
import org.mockito.kotlin.verify
import org.mockito.kotlin.whenever
import io.reactivex.Completable
import io.reactivex.Single
import io.reactivex.schedulers.Schedulers
import org.junit.Test
import java.util.UUID
import java.util.concurrent.Semaphore

class GattCharacteristicWriterTest {

    private val mockServiceId = UUID.fromString("FFFFFFFF-FFFF-FFFF-FFFF-FFFFFFFFFFF1")
    private val mockCharacteristicId = UUID.fromString("FFFFFFFF-FFFF-FFFF-FFFF-FFFFFFFFFFF2")

    private val mockBluetoothGattService = mock<BluetoothGattService>()
    private val mockBluetoothGattCharacteristic = mock<BluetoothGattCharacteristic>()
    private val mockGattTransaction: GattClientTransaction = mock()
    private val mockClientTransactionProvider: WriteGattCharacteristicTransactionProvider = mock {
        on { provide(any(), any()) } doReturn Single.just(mockGattTransaction)
    }
    private val mockGattClientConnectionStateSetter = mock<GattClientConnectionStateSetter> {
        on { set(mockGattConnection, GattState.IDLE) } doReturn Completable.complete()
    }

    private val mockReadWriteLock: Semaphore = mock()

    private val writer = GattCharacteristicWriter(
        gattConnection = mockGattConnection,
        writeTransactionProvider = mockClientTransactionProvider,
        gattClientConnectionStateSetter = mockGattClientConnectionStateSetter,
        scheduler = Schedulers.trampoline(),
        readWriteLock = mockReadWriteLock
    )

    @Test
    fun shouldWriteIfGattTransactionSucceeds() {
        mockGattServiceFound()
        mockGattCharacteristicFound()
        mockTransactionSuccess()

        writer.write(mockServiceId, mockCharacteristicId, ByteArray(1))
            .test()
            .assertComplete()
    }

    @Test
    fun shouldFailToWriteIfGattServiceNotFound() {
        mockGattServiceNotFound()
        mockGattCharacteristicFound()
        mockTransactionSuccess()

        writer.write(mockServiceId, mockCharacteristicId, ByteArray(1))
            .test()
            .assertError { it is GattServiceNotFoundException }
    }

    @Test
    fun shouldFailToWriteIfGattCharacteristicNotFound() {
        mockGattServiceFound()
        mockGattCharacteristicNotFound()
        mockTransactionSuccess()

        writer.write(mockServiceId, mockCharacteristicId, ByteArray(1))
            .test()
            .assertError { it is GattCharacteristicException }
    }

    @Test
    fun shouldFailToWriteIfGattTransactionFails() {
        mockGattServiceFound()
        mockGattCharacteristicFound()
        mockTransactionFailure()

        writer.write(mockServiceId, mockCharacteristicId, ByteArray(1))
            .test()
            .assertError { it is GattTransactionException }
    }

    @Test
    fun shouldReleaseLockAfterSuccessfulWrite() {
        mockGattServiceFound()
        mockGattCharacteristicFound()
        mockTransactionSuccess()

        writer.write(mockServiceId, mockCharacteristicId, ByteArray(1))
            .test()

        verify(mockReadWriteLock).release()
    }

    @Test
    fun shouldReleaseLockAfterFailedWrite() {
        mockGattServiceFound()
        mockGattCharacteristicFound()
        mockTransactionFailure()

        writer.write(mockServiceId, mockCharacteristicId, ByteArray(1))
            .test()

        verify(mockReadWriteLock).release()
    }

    @Test
    fun shouldReleaseLockIfDisposedBeforeTransactionCompletion() {
        mockGattServiceFound()
        mockGattCharacteristicFound()
        // transaction never completes

        writer.write(mockServiceId, mockCharacteristicId, ByteArray(1))
            .test()
            .dispose()

        verify(mockReadWriteLock).release()
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

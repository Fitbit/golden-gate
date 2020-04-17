// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.bluetooth.fbgatt.rx.server

import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattService
import com.fitbit.bluetooth.fbgatt.FitbitBluetoothDevice
import com.fitbit.bluetooth.fbgatt.GattServerTransaction
import com.fitbit.bluetooth.fbgatt.TransactionResult
import com.fitbit.bluetooth.fbgatt.rx.GattCharacteristicException
import com.fitbit.bluetooth.fbgatt.rx.GattServiceNotFoundException
import com.fitbit.bluetooth.fbgatt.rx.GattTransactionException
import com.fitbit.bluetooth.fbgatt.rx.mockBluetoothGattServer
import com.fitbit.bluetooth.fbgatt.rx.mockFitbitGatt
import com.fitbit.bluetooth.fbgatt.rx.mockGattTransactionCompletion
import com.nhaarman.mockitokotlin2.any
import com.nhaarman.mockitokotlin2.doReturn
import com.nhaarman.mockitokotlin2.mock
import com.nhaarman.mockitokotlin2.verify
import com.nhaarman.mockitokotlin2.whenever
import io.reactivex.Single
import io.reactivex.schedulers.Schedulers
import org.junit.Test
import java.util.UUID
import java.util.concurrent.Semaphore

class GattCharacteristicNotifierTest {

    private val mockServiceId = UUID.fromString("FFFFFFFF-FFFF-FFFF-FFFF-FFFFFFFFFFF1")
    private val mockCharacteristicId = UUID.fromString("FFFFFFFF-FFFF-FFFF-FFFF-FFFFFFFFFFF2")

    private val mockFitbitBluetoothDevice = mock<FitbitBluetoothDevice>()
    private val mockBluetoothGattCharacteristic = mock<BluetoothGattCharacteristic>()
    private val mockBluetoothGattService = mock<BluetoothGattService>()
    private val mockGattTransaction = mock<GattServerTransaction>()
    private val mockNotifyTransactionProvider = mock<NotifyGattServerCharacteristicTransactionProvider> {
        on { provide(any(), any(), any()) } doReturn Single.just(mockGattTransaction)
    }
    private val mockNotifyLock: Semaphore = mock()

    private val notifier = GattCharacteristicNotifier(
            fitbitDevice = mockFitbitBluetoothDevice,
            bitgatt = mockFitbitGatt,
            notifyTransactionProvider = mockNotifyTransactionProvider,
            scheduler = Schedulers.trampoline(),
            notifierLock = mockNotifyLock
    )

    @Test
    fun shouldNotifyIfGattTransactionSucceeds() {
        mockGattServiceFound()
        mockGattCharacteristicFound()
        mockTransactionSuccess()

        notifier.notify(mockServiceId, mockCharacteristicId, ByteArray(1))
                .test()
                .assertComplete()
    }

    @Test
    fun shouldFailToNotifyIfTxGattCharacteristicIsNull() {
        mockGattServiceFound()
        mockGattCharacteristicNotFound()
        mockTransactionSuccess()

        notifier.notify(mockServiceId, mockCharacteristicId, ByteArray(1))
                .test()
                .assertError { it is GattCharacteristicException }
    }

    @Test
    fun shouldFailToNotifyIfGattLinkServiceIsNull() {
        mockGattServiceNotFound()
        mockGattCharacteristicFound()
        mockTransactionSuccess()

        notifier.notify(mockServiceId, mockCharacteristicId, ByteArray(1))
                .test()
                .assertError { it is GattServiceNotFoundException }
    }

    @Test
    fun shouldFailToNotifyIfGattTransactionFails() {
        mockGattServiceFound()
        mockGattCharacteristicFound()
        mockTransactionFailure()

        notifier.notify(mockServiceId, mockCharacteristicId, ByteArray(1))
                .test()
                .assertError { it is GattTransactionException }
    }

    @Test
    fun shouldReleaseLockAfterSuccessfulNotify() {
        mockGattServiceFound()
        mockGattCharacteristicFound()
        mockTransactionSuccess()

        notifier.notify(mockServiceId, mockCharacteristicId, ByteArray(1))
            .test()

        verify(mockNotifyLock).release()
    }

    @Test
    fun shouldReleaseLockAfterFailedNotify() {
        mockGattServiceFound()
        mockGattCharacteristicFound()
        mockTransactionFailure()

        notifier.notify(mockServiceId, mockCharacteristicId, ByteArray(1))
            .test()

        verify(mockNotifyLock).release()
    }

    @Test
    fun shouldReleaseLockIfDisposedBeforeTransactionCompletion() {
        mockGattServiceFound()
        mockGattCharacteristicFound()
        // transaction never completes

        notifier.notify(mockServiceId, mockCharacteristicId, ByteArray(1))
            .test()
            .dispose()

        verify(mockNotifyLock).release()
    }

    private fun mockGattServiceFound() =
        whenever(mockBluetoothGattServer.getService(mockServiceId)).thenReturn(mockBluetoothGattService)

    private fun mockGattServiceNotFound() =
        whenever(mockBluetoothGattServer.getService(mockServiceId)).thenReturn(null)

    private fun mockGattCharacteristicFound() =
        whenever(mockBluetoothGattService.getCharacteristic(mockCharacteristicId)).thenReturn(mockBluetoothGattCharacteristic)

    private fun mockGattCharacteristicNotFound() =
        whenever(mockBluetoothGattService.getCharacteristic(mockCharacteristicId)).thenReturn(null)

    private fun mockTransactionSuccess() =
        mockGattTransaction.mockGattTransactionCompletion(TransactionResult.TransactionResultStatus.SUCCESS)

    private fun mockTransactionFailure() =
        mockGattTransaction.mockGattTransactionCompletion(TransactionResult.TransactionResultStatus.FAILURE)

}

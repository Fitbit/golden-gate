// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.bluetooth.fbgatt.rx.server

import android.bluetooth.BluetoothGattServer
import android.bluetooth.BluetoothGattService
import com.fitbit.bluetooth.fbgatt.FitbitGatt
import com.fitbit.bluetooth.fbgatt.GattServerConnection
import com.fitbit.bluetooth.fbgatt.TransactionResult.TransactionResultStatus
import com.fitbit.bluetooth.fbgatt.rx.GattTransactionException
import com.fitbit.bluetooth.fbgatt.rx.mockFitbitGatt
import com.fitbit.bluetooth.fbgatt.rx.mockGattServerConnection
import com.fitbit.bluetooth.fbgatt.rx.mockGattTransactionCompletion
import com.fitbit.bluetooth.fbgatt.tx.AddGattServerServiceTransaction
import org.mockito.kotlin.any
import org.mockito.kotlin.doReturn
import org.mockito.kotlin.eq
import org.mockito.kotlin.mock
import org.mockito.kotlin.never
import org.mockito.kotlin.verify
import org.mockito.kotlin.whenever
import io.reactivex.Single
import java.util.UUID
import kotlin.test.assertTrue
import org.junit.Assert.assertFalse
import org.junit.Test

class BitGattServerTest {

    private val mockUUID = UUID.fromString("FFFFFFFF-FFFF-FFFF-FFFF-FFFFFFFFFFFF")
    private val mockTransaction = mock<AddGattServerServiceTransaction>()
    private val mockTransactionProvider = mock<ServerTransactionProvider> {
        on { getAddServicesTransaction(any(), any()) } doReturn mockTransaction
    }
    private val mockService = mock<BluetoothGattService> {
        on { uuid } doReturn mockUUID
    }
    private val mockGetGattServerServices = mock<GetGattServerServices>()

    private val server = BitGattServer(
        fitbitGatt = mockFitbitGatt,
        serverTransactionProvider = mockTransactionProvider,
        getGattServerServices = { mockGetGattServerServices }
    )

    @Test
    fun shouldReturnServerUpIfServerNotNull() {
        assertTrue(server.isUp())
    }

    @Test
    fun shouldReturnServerDownIfServerNull() {
        assertFalse(mockNullGattServer().isUp())
    }

    @Test
    fun shouldAddServiceIfServiceDoesNotExistAndGattTransactionSucceeds() {
        whenever(mockGetGattServerServices.get()).thenReturn(Single.just(emptyList()))
        mockTransaction.mockGattTransactionCompletion(TransactionResultStatus.SUCCESS)

        server.addServices(mockService)
            .test()
            .assertComplete()

        verify(mockGattServerConnection).runTx(eq(mockTransaction), any())
    }

    @Test
    fun shouldNotAddServiceIfServiceDoesNotExistAndGattTransactionFails() {
        whenever(mockGetGattServerServices.get()).thenReturn(Single.just(emptyList()))
        mockTransaction.mockGattTransactionCompletion(TransactionResultStatus.FAILURE)

        server.addServices(mockService)
            .test()
            .assertError { it is GattTransactionException }

        verify(mockGattServerConnection).runTx(eq(mockTransaction), any())
    }

    @Test
    fun shouldNotAddServiceIfServiceAlreadyExists() {
        whenever(mockGetGattServerServices.get()).thenReturn(Single.just(listOf(mockService)))

        server.addServices(mockService)
            .test()
            .assertComplete()

        verify(mockGattServerConnection, never()).runTx(eq(mockTransaction), any())
    }

    private fun mockNullGattServer(): BitGattServer {
        val nullServer: BluetoothGattServer? = null
        val mockGattServerConnection = mock<GattServerConnection> {
            on { server } doReturn nullServer
        }
        val mockFitbitGatt = mock<FitbitGatt> {
            on { server } doReturn mockGattServerConnection
        }
        return BitGattServer(
            fitbitGatt = mockFitbitGatt,
            serverTransactionProvider = mockTransactionProvider
        )
    }

}

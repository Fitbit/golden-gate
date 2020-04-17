// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.bluetooth.fbgatt.rx.server

import android.bluetooth.BluetoothGattServer
import android.bluetooth.BluetoothGattService
import com.fitbit.bluetooth.fbgatt.FitbitGatt
import com.fitbit.bluetooth.fbgatt.GattServerConnection
import com.fitbit.bluetooth.fbgatt.TransactionResult.TransactionResultStatus
import com.fitbit.bluetooth.fbgatt.rx.GattServerNotFoundException
import com.fitbit.bluetooth.fbgatt.rx.GattTransactionException
import com.fitbit.bluetooth.fbgatt.rx.mockBluetoothGattServer
import com.fitbit.bluetooth.fbgatt.rx.mockFitbitGatt
import com.fitbit.bluetooth.fbgatt.rx.mockGattServerConnection
import com.fitbit.bluetooth.fbgatt.rx.mockGattTransactionCompletion
import com.fitbit.bluetooth.fbgatt.tx.AddGattServerServiceTransaction
import com.nhaarman.mockitokotlin2.any
import com.nhaarman.mockitokotlin2.doReturn
import com.nhaarman.mockitokotlin2.eq
import com.nhaarman.mockitokotlin2.mock
import com.nhaarman.mockitokotlin2.never
import com.nhaarman.mockitokotlin2.verify
import com.nhaarman.mockitokotlin2.whenever
import org.junit.Assert.assertFalse
import org.junit.Test
import java.util.UUID
import kotlin.test.assertTrue

class BitGattServerTest {

    private val mockUUID = UUID.fromString("FFFFFFFF-FFFF-FFFF-FFFF-FFFFFFFFFFFF")
    private val mockTransaction = mock<AddGattServerServiceTransaction>()
    private val mockTransactionProvider = mock<ServerTransactionProvider> {
        on { getAddServicesTransaction(any(), any()) } doReturn mockTransaction
    }
    private val mockService = mock<BluetoothGattService>{
        on { uuid } doReturn mockUUID
    }

    private val server = BitGattServer(
            fitbitGatt = mockFitbitGatt,
            serverTransactionProvider = mockTransactionProvider
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
        whenever(mockBluetoothGattServer.getService(mockUUID)).thenReturn(null)
        mockTransaction.mockGattTransactionCompletion(TransactionResultStatus.SUCCESS)

        server.addServices(mockService)
                .test()
                .assertComplete()

        verify(mockGattServerConnection).runTx(eq(mockTransaction), any())
    }

    @Test
    fun shouldNotAddServiceIfServiceDoesNotExistAndGattTransactionFails() {
        whenever(mockBluetoothGattServer.getService(mockUUID)).thenReturn(null)
        mockTransaction.mockGattTransactionCompletion(TransactionResultStatus.FAILURE)

        server.addServices(mockService)
                .test()
                .assertError {it is GattTransactionException }

        verify(mockGattServerConnection).runTx(eq(mockTransaction), any())
    }

    @Test
    fun shouldNotAddServiceIfServiceAlreadyExists() {
        whenever(mockBluetoothGattServer.getService(mockUUID)).thenReturn(mockService)

        server.addServices(mockService)
                .test()
                .assertComplete()

        verify(mockGattServerConnection, never()).runTx(eq(mockTransaction), any())
    }

    @Test
    fun shouldNotAddServiceIfServerIsNull() {
        mockNullGattServer().addServices(mockService)
                .test()
                .assertError {it is GattServerNotFoundException }

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

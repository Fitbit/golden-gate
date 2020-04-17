// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bt

import android.bluetooth.BluetoothGatt
import com.fitbit.bluetooth.fbgatt.rx.client.BitGattPeripheral
import com.nhaarman.mockitokotlin2.any
import com.nhaarman.mockitokotlin2.mock
import com.nhaarman.mockitokotlin2.never
import com.nhaarman.mockitokotlin2.verify
import com.nhaarman.mockitokotlin2.whenever
import io.reactivex.Single
import org.junit.Before
import org.junit.Test

class PeripheralConnectorTest {

    private val mockBluetoothGatt = mock<BluetoothGatt>()
    private val mockPeripheral = mock<BitGattPeripheral>()

    private val connection = PeripheralConnector(
            bluetoothAddress = mockBluetoothAddress,
            fitbitGatt = mockFitbitGatt,
            peripheralProvider = { mockPeripheral }
    )

    @Before
    fun setup() {
        mockConnectSuccess()
    }

    @Test
    fun shouldConnectIfDeviceIsKnown() {
        mockFitbitGatt.mockDeviceKnown(true)

        connection.connect()
                .test()
                .assertValue { it == mockGattConnection }

        verify(mockPeripheral).connect()
    }

    @Test
    fun shouldNotConnectIfDeviceIsNotKnown() {
        mockFitbitGatt.mockDeviceKnown(false)

        connection.connect()
                .test()
                .assertError { it is NoSuchElementException }

        verify(mockPeripheral, never()).connect()
        verify(mockPeripheral, never()).requestConnectionPriority(BluetoothGatt.CONNECTION_PRIORITY_HIGH)
    }

    @Test
    fun shouldFailToConnectIfDeviceBleConnectionFails() {
        mockFitbitGatt.mockDeviceKnown(true)
        mockConnectFailure()

        connection.connect()
                .test()
                .assertError { it is Exception }

        verify(mockPeripheral).connect()
    }

    private fun mockConnectSuccess() {
        whenever(mockPeripheral.connect()).thenReturn(Single.just(mockBluetoothGatt))
    }

    private fun mockConnectFailure() {
        whenever(mockPeripheral.connect()).thenReturn(Single.error(Exception("Failed")))
    }
}

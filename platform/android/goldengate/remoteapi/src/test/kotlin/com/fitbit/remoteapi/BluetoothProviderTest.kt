// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.remoteapi

import android.content.Context
import com.fitbit.bluetooth.fbgatt.FitbitBluetoothDevice
import com.fitbit.bluetooth.fbgatt.GattConnection
import com.fitbit.bluetooth.fbgatt.rx.scanner.PeripheralScanner
import org.mockito.kotlin.any
import org.mockito.kotlin.doReturn
import org.mockito.kotlin.eq
import org.mockito.kotlin.mock
import io.reactivex.Observable
import io.reactivex.schedulers.Schedulers
import io.reactivex.schedulers.TestScheduler
import org.junit.Test
import org.junit.runner.RunWith
import org.junit.runners.JUnit4
import java.util.concurrent.TimeUnit
import kotlin.test.assertEquals
import kotlin.test.fail

@RunWith(JUnit4::class)
class BluetoothProviderTest {

    val macAddress = "00:11:22:33:44:55"
    val context = mock<Context>()
    val fitbitBluetoothDevice = mock<FitbitBluetoothDevice> {
        on { address } doReturn macAddress
    }
    val connection = mock<GattConnection> {
        on { getDevice() } doReturn fitbitBluetoothDevice
    }

    @Test
    fun getRxBlePeripheral() {
        val mockPeripheralScanner = mock<PeripheralScanner> {
            on {
                scanForDevices(
                    eq(context),
                    any(),
                    any(),
                    any()
                )
            } doReturn Observable.just<PeripheralScanner.ScanEvent>(
                PeripheralScanner.ScanEvent.Discovered(
                    connection
                )
            )
        }

        val bluetoothProvider = BluetoothProvider({ mockPeripheralScanner })

        val returnedConnection = bluetoothProvider.bluetoothDevice(context, macAddress)

        assertEquals(connection, returnedConnection)
    }

    @Test(expected = RuntimeException::class)
    fun bluetoothDeviceTimesOut() {
        val mockPeripheralScanner = mock<PeripheralScanner> {
            on {
                scanForDevices(
                    eq(context),
                    any(),
                    any()
                )
            } doReturn Observable.never()
        }

        val testScheduler = TestScheduler()
        val bluetoothProvider = BluetoothProvider({ mockPeripheralScanner }, testScheduler)

        //This is a hack because blockingFirst() with a timeout is pretty hard to test.
        Schedulers.computation().scheduleDirect(
            { testScheduler.advanceTimeBy(35, TimeUnit.SECONDS) },
            25,
            TimeUnit.MILLISECONDS
        )
        val returnedConnection = bluetoothProvider.bluetoothDevice(context, macAddress)


        fail("Expected $returnedConnection to not be populated since the above method should have thrown NoSuchElementException")
    }
}

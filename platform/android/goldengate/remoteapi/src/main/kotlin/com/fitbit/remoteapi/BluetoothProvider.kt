// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.remoteapi

import android.content.Context
import com.fitbit.bluetooth.fbgatt.GattConnection
import com.fitbit.bluetooth.fbgatt.rx.scanner.DeviceAddressPeripheralScannerFilter
import com.fitbit.bluetooth.fbgatt.rx.scanner.PeripheralScanner
import io.reactivex.Scheduler
import io.reactivex.schedulers.Schedulers
import java.util.concurrent.TimeUnit

//The websocket times out after 40 seconds, so we should return an error before that.
private const val REMOTE_API_SCAN_TIMEOUT_SECONDS: Long = 35

/**
 * Helper object to return a device by name, whether already connected or not
 */
class BluetoothProvider(
    private val peripheralScannerProvider: () -> PeripheralScanner = { PeripheralScanner() },
    private val scheduler: Scheduler = Schedulers.computation()
) {

    /**
     * @param macAddress the name of the device
     *
     * @return A bluetooth device with the given [targetDeviceName] or null if it is not found
     */
    fun bluetoothDevice(context: Context, macAddress: String): GattConnection {
        return peripheralScannerProvider().scanForDevices(
            context,
            listOf(DeviceAddressPeripheralScannerFilter(macAddress)),
            true
        )
            .map { it.connection }.take(1)
            .timeout(REMOTE_API_SCAN_TIMEOUT_SECONDS, TimeUnit.SECONDS, scheduler)
            .blockingFirst()
    }
}

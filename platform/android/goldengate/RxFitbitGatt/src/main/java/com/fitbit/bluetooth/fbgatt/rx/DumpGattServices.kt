// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.bluetooth.fbgatt.rx

import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattDescriptor
import android.bluetooth.BluetoothGattService
import timber.log.Timber

/**
 * Logs warning level logs for available GATT services and characteristics
 */
fun dumpServicesWarning(services: List<BluetoothGattService>?) {
    dumpServices(services) { message, args -> Timber.w(message, args) }
}

/**
 * Logs logs (debug level by default) for available GATT services and characteristics
 */
fun dumpServices(
    services: List<BluetoothGattService>?,
    log: (String, Any?) -> Unit = { message, args -> Timber.d(message, args) }
) {
    services?.let{
        for (service: BluetoothGattService in it) {
            log("Service UUID %s", service.uuid)
            for (characteristic: BluetoothGattCharacteristic in service.characteristics) {
                log("  Characteristic %s found", characteristic.uuid)
                for (descriptor: BluetoothGattDescriptor in characteristic.descriptors) {
                    log("    Descriptor %s found", descriptor.uuid)
                }
            }
        }
    } ?: Timber.d("No services.")
}

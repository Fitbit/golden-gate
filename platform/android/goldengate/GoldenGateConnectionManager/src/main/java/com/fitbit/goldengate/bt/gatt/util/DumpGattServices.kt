// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bt.gatt.util

import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattDescriptor
import android.bluetooth.BluetoothGattService
import timber.log.Timber

fun dumpServices(services: List<BluetoothGattService>?) {
    services?.let{
        for (service: BluetoothGattService in it) {
            Timber.d("Service UUID %s", service.uuid)
            for (characteristic: BluetoothGattCharacteristic in service.characteristics) {
                Timber.d("  Characteristic %s found", characteristic.uuid)
                for (descriptor: BluetoothGattDescriptor in characteristic.descriptors) {
                    Timber.d("    Descriptor %s found", descriptor.uuid)
                }
            }
        }
    } ?: Timber.d("No services.")
}

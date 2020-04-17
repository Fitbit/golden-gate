// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bt.gatt.server.services.gattlink

import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattDescriptor
import com.fitbit.bluetooth.fbgatt.rx.CLIENT_CONFIG_UUID
import java.util.UUID

/**
 * Represents a GattLink Tx Characteristic, which is used for transmitting data to peripheral
 */
class TransmitCharacteristic : BluetoothGattCharacteristic(
        uuid,
        BluetoothGattCharacteristic.PROPERTY_READ or BluetoothGattCharacteristic.PROPERTY_NOTIFY,
        BluetoothGattCharacteristic.PERMISSION_READ
) {
    init {
        val descriptor = BluetoothGattDescriptor(
                CLIENT_CONFIG_UUID,
                BluetoothGattDescriptor.PERMISSION_READ or BluetoothGattDescriptor.PERMISSION_WRITE
        )
        addDescriptor(descriptor)
    }

    companion object {
        val uuid: UUID = UUID.fromString("ABBAFF02-E56A-484C-B832-8B17CF6CBFE8")
    }
}

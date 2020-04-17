// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bt.manager

import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCharacteristic
import android.content.Context
import org.mockito.Mockito
import java.util.UUID

/**
 * Created by muyttersprot on 12/6/17.
 */
class MockUtil {
    companion object {
        fun getMockCharacteristic(address: String): BluetoothGattCharacteristic {
            val char = Mockito.mock(BluetoothGattCharacteristic::class.java)
            Mockito.`when`(char.uuid).thenReturn(UUID.fromString(address))
            return char
        }

        fun getMockBluetoothDevice(address: String) : BluetoothDevice {
            val device = Mockito.mock(BluetoothDevice::class.java)
            Mockito.`when`(device.address).thenReturn(address)
            return device
        }

        fun getMockContext() : Context = Mockito.mock(Context::class.java)

        fun getMockGatt() : BluetoothGatt = Mockito.mock(BluetoothGatt::class.java)
    }
}

package com.fitbit.bluetooth.fbgatt.rx.server.listeners

import android.bluetooth.BluetoothDevice
import com.fitbit.bluetooth.fbgatt.GattServerConnection
import com.fitbit.bluetooth.fbgatt.ServerConnectionEventListener
import com.fitbit.bluetooth.fbgatt.TransactionResult

/**
 * Base [ServerConnectionEventListener] that provides no operations when one of the callback is called
 */
interface BaseServerConnectionEventListener: ServerConnectionEventListener {

    override fun onServerMtuChanged(device: BluetoothDevice, result: TransactionResult, connection: GattServerConnection) {
    }

    override fun onServerCharacteristicWriteRequest(device: BluetoothDevice, result: TransactionResult, connection: GattServerConnection) {
    }

    override fun onServerDescriptorWriteRequest(device: BluetoothDevice, result: TransactionResult, connection: GattServerConnection) {
    }

    override fun onServerCharacteristicReadRequest(device: BluetoothDevice, result: TransactionResult, connection: GattServerConnection) {
    }

    override fun onServerDescriptorReadRequest(device: BluetoothDevice, result: TransactionResult, connection: GattServerConnection) {
    }

    override fun onServerConnectionStateChanged(device: BluetoothDevice, result: TransactionResult, connection: GattServerConnection) {
    }
}

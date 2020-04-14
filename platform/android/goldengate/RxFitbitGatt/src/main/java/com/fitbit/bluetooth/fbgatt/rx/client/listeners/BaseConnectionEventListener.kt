package com.fitbit.bluetooth.fbgatt.rx.client.listeners

import com.fitbit.bluetooth.fbgatt.ConnectionEventListener
import com.fitbit.bluetooth.fbgatt.GattConnection
import com.fitbit.bluetooth.fbgatt.TransactionResult

/**
 * Base [ConnectionEventListener] that provides no operations when one of the callback is called
 */
interface BaseConnectionEventListener: ConnectionEventListener {

    override fun onPhyChanged(result: TransactionResult, connection: GattConnection) {
    }

    override fun onServicesDiscovered(result: TransactionResult, connection: GattConnection) {
    }

    override fun onMtuChanged(result: TransactionResult, connection: GattConnection) {
    }

    override fun onClientCharacteristicChanged(result: TransactionResult, connection: GattConnection) {
    }

    override fun onClientConnectionStateChanged(result: TransactionResult, connection: GattConnection) {
    }
}

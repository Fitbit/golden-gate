package com.fitbit.goldengate.bindings.node

import com.fitbit.goldengate.bindings.util.checkBluetoothAddress


/**
 * Uniquely identifies a Node with a bluetooth address that is connected over BLE
 */
data class BluetoothAddressNodeKey(
        override val value: String
) : NodeKey<String> {

    init {
        verifyBluetoothAddress()
    }

    private fun verifyBluetoothAddress() {
        if(!checkBluetoothAddress(value)) {
            throw IllegalArgumentException("Invalid BluetoothAddress: $value")
        }
    }

}

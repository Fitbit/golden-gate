package com.fitbit.goldengate.bt.gatt.server.services.gattlink

import android.bluetooth.BluetoothGattCharacteristic
import java.util.UUID

/**
 * Represents a GattLink Rx Characteristic, which is used for receiving data from peripheral
 */
class ReceiveCharacteristic : BluetoothGattCharacteristic(
        uuid,
        BluetoothGattCharacteristic.PROPERTY_WRITE_NO_RESPONSE,
        BluetoothGattCharacteristic.PERMISSION_WRITE
) {
    companion object {
        val uuid: UUID = UUID.fromString("ABBAFF01-E56A-484C-B832-8B17CF6CBFE8")
    }

}

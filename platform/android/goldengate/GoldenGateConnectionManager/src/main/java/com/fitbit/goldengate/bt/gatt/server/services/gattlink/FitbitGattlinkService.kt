package com.fitbit.goldengate.bt.gatt.server.services.gattlink

import android.bluetooth.BluetoothGattService
import java.util.UUID

/**
 * Represents the Gattlink Service hosted on mobile that allows reliable streaming of
 * arbitrary data over a BLE connection using the GATT protocol, with support for BLE/GATT stacks
 * that don't always have the necessary support for writing GATT characteristics back-to-back
 * without dropping data
 *
 * Fitbit has registered the Gattlink Service to SIG with 16-bit UUID 0xFD62, which has a different
 * service UUID from original one.
 * https://www.bluetooth.com/specifications/assigned-numbers/16-bit-uuids-for-members/
 */
class FitbitGattlinkService : BluetoothGattService(
    uuid,
    BluetoothGattService.SERVICE_TYPE_PRIMARY) {

    init {
        addCharacteristic(ReceiveCharacteristic())
        addCharacteristic(TransmitCharacteristic())
    }

    companion object {
        val uuid: UUID = UUID.fromString("0000FD62-0000-1000-8000-00805F9B34FB")
    }
}

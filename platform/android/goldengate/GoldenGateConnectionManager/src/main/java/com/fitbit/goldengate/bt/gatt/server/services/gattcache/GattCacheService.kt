package com.fitbit.goldengate.bt.gatt.server.services.gattcache

import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattService
import java.util.UUID

/**
 * Fitbit GATT cache service, hosted by node; Hub uses this service to verify if
 * cached peer GATT service database is up-to-date.
 * https://wiki.fitbit.com/display/firmware/GATT+Service+Change+Indication
 */
class GattCacheService : BluetoothGattService(
    uuid,
    BluetoothGattService.SERVICE_TYPE_PRIMARY
) {
    companion object {
        val uuid: UUID = UUID.fromString("AC2F0045-8182-4BE5-91E0-2992E6B40EBB")
    }

    init {
        addCharacteristic(EphemeralCharacteristicPointer())
        addCharacteristic(EphemeralCharacteristic())
    }
}

class EphemeralCharacteristicPointer: BluetoothGattCharacteristic(
    uuid,
    BluetoothGattCharacteristic.PROPERTY_READ,
    BluetoothGattCharacteristic.PERMISSION_READ
) {
    companion object {
        val uuid: UUID = UUID.fromString("AC2F0145-8182-4BE5-91E0-2992E6B40EBB")
    }
}

class EphemeralCharacteristic: BluetoothGattCharacteristic(
    uuid,
    BluetoothGattCharacteristic.PROPERTY_WRITE,
    BluetoothGattCharacteristic.PERMISSION_WRITE
) {
    companion object {
        val uuid: UUID = UUID.fromString("AC2F0345-8182-4BE5-91E0-2992E6B40EBB")
    }
}

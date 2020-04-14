package com.fitbit.linkcontroller.services.configuration

import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattDescriptor
import android.bluetooth.BluetoothGattService
import com.fitbit.bluetooth.fbgatt.rx.CLIENT_CONFIG_UUID
import java.util.*

/**
 * Represents a Bluetooth GATT Link Configuration Service hosted on mobile that allows setting up
 * different connection parameters by the mobile between the peripheral device and mobile or
 * general purpose command, such as BLE disconnection request.
 */
class LinkConfigurationService : BluetoothGattService(
    uuid,
    BluetoothGattService.SERVICE_TYPE_PRIMARY
) {

    companion object {
        val uuid = UUID.fromString("ABBAFC00-E56A-484C-B832-8B17CF6CBFE8")

    }

    init {
        addCharacteristic(ClientPreferredConnectionConfigurationCharacteristic())
        addCharacteristic(ClientPreferredConnectionModeCharacteristic())
        addCharacteristic(GeneralPurposeCommandCharacteristic())
    }
}

/**
 * This characteristic is used by the mobile app to setup different connection parameters
 */
class ClientPreferredConnectionConfigurationCharacteristic : BluetoothGattCharacteristic(
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
        val uuid: UUID = UUID.fromString("ABBAFC01-E56A-484C-B832-8B17CF6CBFE8")
    }
}

/**
 * This characteristic is used by the mobile app to setup connection mode
 */
class ClientPreferredConnectionModeCharacteristic : BluetoothGattCharacteristic(
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
        val uuid: UUID = UUID.fromString("ABBAFC02-E56A-484C-B832-8B17CF6CBFE8")
    }
}

/**
 * This characteristic is used by the mobile app to issue General Purpose Command
 */
class GeneralPurposeCommandCharacteristic : BluetoothGattCharacteristic(
    uuid,
    BluetoothGattCharacteristic.PROPERTY_NOTIFY,
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
        val uuid: UUID = UUID.fromString("ABBAFC03-E56A-484C-B832-8B17CF6CBFE8")
    }
}

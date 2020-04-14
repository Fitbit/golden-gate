package com.fitbit.goldengate.bt.gatt.client.services

import java.util.UUID

/**
 * Generic Attribute service, hosted by node
 * This is a BLE public service defined by SIG https://www.bluetooth.com/specifications/gatt/services/
 * Mainly for receiving service changed indication
 */
class GenericAttributeService {
    companion object {
        val uuid: UUID = UUID.fromString("00001801-0000-1000-8000-00805f9b34fb")
    }

    class ServiceChangedCharacteristic {
        companion object {
            val uuid: UUID = UUID.fromString("00002a05-0000-1000-8000-00805f9b34fb")
        }
    }
}
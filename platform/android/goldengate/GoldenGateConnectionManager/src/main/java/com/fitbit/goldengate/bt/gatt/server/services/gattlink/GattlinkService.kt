// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bt.gatt.server.services.gattlink

import android.bluetooth.BluetoothGattService
import java.util.UUID

/**
 * Represents a Bluetooth GATT Link Service hosted on mobile that allows reliable streaming of
 * arbitrary data over a BLE connection using the GATT protocol, with support for BLE/GATT stacks
 * that don't always have the necessary support for writing GATT characteristics back-to-back
 * without dropping data
 */
class GattlinkService : BluetoothGattService(
        uuid,
        BluetoothGattService.SERVICE_TYPE_PRIMARY) {

    init {
        addCharacteristic(ReceiveCharacteristic())
        addCharacteristic(TransmitCharacteristic())
    }

    companion object {
        val uuid: UUID = UUID.fromString("ABBAFF00-E56A-484C-B832-8B17CF6CBFE8")
    }
}

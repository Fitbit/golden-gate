// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.node

import com.fitbit.bluetooth.fbgatt.FitbitGatt
import com.fitbit.bluetooth.fbgatt.GattConnection
import com.fitbit.bluetooth.fbgatt.rx.server.GattCharacteristicNotifier
import com.fitbit.goldengate.bt.gatt.server.services.gattlink.GattlinkService
import com.fitbit.goldengate.bt.gatt.server.services.gattlink.TransmitCharacteristic
import io.reactivex.Completable

/**
 * Use Gattlink service hosted on local GATT server for sending data via its Tx characteristic
 */
internal class LocalGattlinkNodeDataSender(
    private val connection: GattConnection,
    fitbitGatt: FitbitGatt = FitbitGatt.getInstance(),
    private val gattCharacteristicNotifier: GattCharacteristicNotifier = GattCharacteristicNotifier(connection.device, fitbitGatt)
) : NodeDataSender {

    override fun send(data: ByteArray): Completable =
        gattCharacteristicNotifier.notify(GattlinkService.uuid, TransmitCharacteristic.uuid, data)
}

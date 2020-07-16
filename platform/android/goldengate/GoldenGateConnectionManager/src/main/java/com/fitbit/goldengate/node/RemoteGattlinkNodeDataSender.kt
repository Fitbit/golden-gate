// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.node

import com.fitbit.bluetooth.fbgatt.GattConnection
import com.fitbit.bluetooth.fbgatt.rx.client.GattCharacteristicWriter
import com.fitbit.goldengate.bt.gatt.server.services.gattlink.ReceiveCharacteristic
import io.reactivex.Completable
import java.util.UUID

/**
 * Use Gattlink service hosted on connected Node GATT server for sending data via its Rx characteristic
 */
internal class RemoteGattlinkNodeDataSender(
    private val connection: GattConnection,
    private val serviceId: UUID,
    private val gattCharacteristicWriter: GattCharacteristicWriter = GattCharacteristicWriter(connection)
) : NodeDataSender {

    override fun send(data: ByteArray): Completable =
        gattCharacteristicWriter.write(
            serviceId,
            ReceiveCharacteristic.uuid, data)
}

// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.node

import com.fitbit.bluetooth.fbgatt.GattConnection
import com.fitbit.bluetooth.fbgatt.rx.client.listeners.GattClientCharacteristicChangeListener
import com.fitbit.goldengate.bt.gatt.server.services.gattlink.TransmitCharacteristic
import io.reactivex.Observable

/**
 * Use Gattlink service hosted on connected Node GATT server for receiving data on its Tx characteristic
 */
internal class RemoteGattlinkNodeDataReceiver(
    private val connection: GattConnection,
    private val gattClientCharacteristicChangeListener: GattClientCharacteristicChangeListener = GattClientCharacteristicChangeListener()
) : NodeDataReceiver {

    override fun receive(): Observable<ByteArray> =
        gattClientCharacteristicChangeListener.register(connection, TransmitCharacteristic.uuid)
}

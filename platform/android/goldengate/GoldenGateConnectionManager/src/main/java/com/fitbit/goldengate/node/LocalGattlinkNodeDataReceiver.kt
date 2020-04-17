// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.node

import com.fitbit.bluetooth.fbgatt.GattConnection
import com.fitbit.goldengate.bt.gatt.server.services.gattlink.listeners.ReceiveCharacteristicDataListener
import io.reactivex.Observable

/**
 * Use Gattlink service hosted on local GATT server for receiving data on its Rx characteristic
 */
internal class LocalGattlinkNodeDataReceiver(
    private val connection: GattConnection,
    private val receiveCharacteristicDataListener: ReceiveCharacteristicDataListener = ReceiveCharacteristicDataListener.instance
) : NodeDataReceiver {

    override fun receive(): Observable<ByteArray> =
        receiveCharacteristicDataListener.register(connection.device.btDevice)

}

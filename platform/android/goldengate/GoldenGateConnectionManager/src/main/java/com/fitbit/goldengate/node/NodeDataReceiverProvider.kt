// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.node

import com.fitbit.bluetooth.fbgatt.GattConnection
import com.fitbit.goldengate.bt.gatt.server.services.gattlink.GattlinkService
import timber.log.Timber

/**
 * Provides [NodeDataReceiver] to use for receiving data
 */
class NodeDataReceiverProvider {

    /**
     * Provides [NodeDataReceiver] to use given [connection]
     */
    fun provide(connection: GattConnection): NodeDataReceiver {
        return if (connection.getRemoteGattService(GattlinkService.uuid) != null) {
            Timber.d("Using GattlinkService hosted from remote device for receiving data")
            RemoteGattlinkNodeDataReceiver(connection)
        } else {
            Timber.d("Using GattlinkService hosted on local device for receiving data")
            LocalGattlinkNodeDataReceiver(connection)
        }
    }

}

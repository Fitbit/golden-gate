// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.node

import com.fitbit.bluetooth.fbgatt.FitbitGatt
import com.fitbit.bluetooth.fbgatt.GattConnection
import com.fitbit.goldengate.bt.gatt.server.services.gattlink.FitbitGattlinkService
import com.fitbit.goldengate.bt.gatt.server.services.gattlink.GattlinkService
import timber.log.Timber

/**
 * Provides [NodeDataSender] to use for sending data
 */
class NodeDataSenderProvider(private val fitbitGatt: FitbitGatt = FitbitGatt.getInstance()) {

    /**
     * Provides [NodeDataSender] to use for sending data for given [connection]
     */
    fun provide(connection: GattConnection): NodeDataSender {
        return when {
            connection.getRemoteGattService(FitbitGattlinkService.uuid) != null -> {
                Timber.d("Using FitbitGattlink Service hosted from remote device for sending data")
                RemoteGattlinkNodeDataSender(connection, FitbitGattlinkService.uuid)
            }
            connection.getRemoteGattService(GattlinkService.uuid) != null -> {
                Timber.d("Using Gattlink Service hosted from remote device for sending data")
                RemoteGattlinkNodeDataSender(connection, GattlinkService.uuid)
            }
            else -> {
                Timber.d("Using FitbitGattlink Service hosted on local device for sending data")
                LocalGattlinkNodeDataSender(connection, FitbitGattlinkService.uuid, fitbitGatt)
            }
        }
    }

}

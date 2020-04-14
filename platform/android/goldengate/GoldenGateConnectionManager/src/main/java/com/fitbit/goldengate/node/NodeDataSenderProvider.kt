package com.fitbit.goldengate.node

import com.fitbit.bluetooth.fbgatt.FitbitGatt
import com.fitbit.bluetooth.fbgatt.GattConnection
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
        return if (connection.getRemoteGattService(GattlinkService.uuid) != null) {
            Timber.d("Using GattlinkService hosted from remote device for sending data")
            RemoteGattlinkNodeDataSender(connection)
        } else {
            Timber.d("Using GattlinkService hosted on local device for sending data")
            LocalGattlinkNodeDataSender(connection, fitbitGatt)
        }
    }

}

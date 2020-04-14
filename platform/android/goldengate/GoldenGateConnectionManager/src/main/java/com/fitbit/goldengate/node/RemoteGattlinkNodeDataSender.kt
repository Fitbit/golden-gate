package com.fitbit.goldengate.node

import com.fitbit.bluetooth.fbgatt.GattConnection
import com.fitbit.bluetooth.fbgatt.rx.client.GattCharacteristicWriter
import com.fitbit.goldengate.bt.gatt.server.services.gattlink.GattlinkService
import com.fitbit.goldengate.bt.gatt.server.services.gattlink.ReceiveCharacteristic
import io.reactivex.Completable

/**
 * Use Gattlink service hosted on connected Node GATT server for sending data via its Rx characteristic
 */
internal class RemoteGattlinkNodeDataSender(
    private val connection: GattConnection,
    private val gattCharacteristicWriter: GattCharacteristicWriter = GattCharacteristicWriter(connection)
) : NodeDataSender {

    override fun send(data: ByteArray): Completable =
        gattCharacteristicWriter.write(GattlinkService.uuid, ReceiveCharacteristic.uuid, data)
}

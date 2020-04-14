package com.fitbit.bluetooth.fbgatt.rx.server

import android.bluetooth.BluetoothGattService
import com.fitbit.bluetooth.fbgatt.FitbitGatt
import com.fitbit.bluetooth.fbgatt.GattServerConnection
import com.fitbit.bluetooth.fbgatt.GattState
import com.fitbit.bluetooth.fbgatt.rx.GattServerNotFoundException
import com.fitbit.bluetooth.fbgatt.rx.runTxReactive
import com.fitbit.bluetooth.fbgatt.tx.AddGattServerServiceTransaction
import io.reactivex.Completable
import timber.log.Timber

/**
 * Reactive API for the Bitgatt Bluetooth GATT Profile server role.
 */
class BitGattServer (
        private val fitbitGatt: FitbitGatt = FitbitGatt.getInstance(),
        private val serverTransactionProvider: ServerTransactionProvider = ServerTransactionProvider()
) {

    /**
     * Returns true if up, false if not up.
     */
    fun isUp(): Boolean = fitbitGatt.server.server != null

    /**
     * Adds a [BluetoothGattService] to this gatt server.
     * @param service The [BluetoothGattService] that will be added
     */
    fun addServices(service: BluetoothGattService): Completable {
        return when {
            fitbitGatt.server.server == null -> Completable.error(GattServerNotFoundException())
            fitbitGatt.server.server.getService(service.uuid) == null -> {
                Timber.d("Adding GATT service ${service.uuid}")
                val tx = serverTransactionProvider.getAddServicesTransaction(fitbitGatt.server, service)
                tx.runTxReactive(fitbitGatt.server).ignoreElement()
            }
            else -> {
                Timber.d("GATT service ${service.uuid} already added")
                Completable.complete()
            }
        }
    }
}

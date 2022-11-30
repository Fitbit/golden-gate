package com.fitbit.bluetooth.fbgatt.rx.client

import android.bluetooth.BluetoothGattService
import com.fitbit.bluetooth.fbgatt.GattClientTransaction
import com.fitbit.bluetooth.fbgatt.GattConnection
import com.fitbit.bluetooth.fbgatt.GattState
import com.fitbit.bluetooth.fbgatt.rx.dumpServices
import com.fitbit.bluetooth.fbgatt.rx.runTxReactive
import com.fitbit.bluetooth.fbgatt.tx.GattClientDiscoverServicesTransaction
import io.reactivex.Single
import timber.log.Timber

/**
 * Discovers GATT service for given peripheral
 */
class GattServiceDiscoverer(
    private val transactionProvider: GattServiceDiscovererTransactionProvider = GattServiceDiscovererTransactionProvider(),
    private val provideBitGattPeer: (GattConnection) -> BitGattPeer = { BitGattPeer(it) }
) {

    /**
     * Performs service discovery on the peer
     * If the gattConnection is not connected we will first try to connect
     *
     * @param gattConnection The GattConnection object
     * @return A [Single] containing a [List] of discovered [BluetoothGattService]s
     */
    fun discover(gattConnection: GattConnection): Single<List<BluetoothGattService>> {
        return provideBitGattPeer(gattConnection).connect()
            .flatMap { transactionProvider.provide(gattConnection) }
            .flatMap { transaction -> transaction.runTxReactive(gattConnection) }
            .map { gattConnection.gatt?.services ?: emptyList() }
            .doOnSubscribe { Timber.d("Discovering services for $gattConnection") }
            .doOnError { Timber.e(it, "Fail to discover services for $gattConnection") }
            .doOnSuccess {
                Timber.d("Successfully discovered services for $gattConnection")
                dumpServices(it)
            }
    }
}

class GattServiceDiscovererTransactionProvider {
    fun provide(gattConnection: GattConnection) : Single<GattClientTransaction> =
        Single.fromCallable { GattClientDiscoverServicesTransaction(gattConnection, GattState.DISCOVERY_SUCCESS) }
}

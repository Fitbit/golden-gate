package com.fitbit.bluetooth.fbgatt.rx.client

import com.fitbit.bluetooth.fbgatt.GattConnection
import com.fitbit.bluetooth.fbgatt.GattState
import com.fitbit.bluetooth.fbgatt.GattTransaction
import com.fitbit.bluetooth.fbgatt.rx.runTxReactive
import com.fitbit.bluetooth.fbgatt.tx.GattClientRefreshGattTransaction
import io.reactivex.Completable
import io.reactivex.Single
import timber.log.Timber

class GattServiceRefresher(
    private val refreshTransactionProvider: GattClientRefreshGattTransactionProvider = GattClientRefreshGattTransactionProvider(),
    private val provideBitGattPeripheral: (GattConnection) -> BitGattPeripheral = { BitGattPeripheral(it) }) {

    /**
     * Performs service refresh on the peripheral
     * If the gattConnection is not connected we will first try to connect
     *
     * @param gattConnection The GattConnection object
     * @return A [Completable] that will complete once refresh has been completed.
     */
    fun refresh(gattConnection: GattConnection): Completable {
        return provideBitGattPeripheral(gattConnection).connect()
            .flatMap { refreshTransactionProvider.provide(gattConnection) }
            .flatMap { transaction -> transaction.runTxReactive(gattConnection) }
            .doOnSubscribe { Timber.d("Refreshing services for $gattConnection") }
            .doOnError { Timber.e(it, "Fail to refreshing services for $gattConnection") }
            .doOnSuccess { Timber.d("Successfully Refreshing services for $gattConnection") }
            .ignoreElement()
    }
}

class GattClientRefreshGattTransactionProvider {
    fun provide(gattConnection: GattConnection) : Single<GattTransaction> =
        Single.fromCallable { GattClientRefreshGattTransaction(gattConnection, GattState.REFRESH_GATT_SUCCESS) }
}
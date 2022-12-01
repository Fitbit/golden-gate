package com.fitbit.bluetooth.fbgatt.rx.server

import android.bluetooth.BluetoothGattService
import com.fitbit.bluetooth.fbgatt.GattServerConnection
import com.fitbit.bluetooth.fbgatt.GattState
import com.fitbit.bluetooth.fbgatt.GattState.GET_SERVER_SERVICES_SUCCESS
import com.fitbit.bluetooth.fbgatt.rx.runTxReactive
import com.fitbit.bluetooth.fbgatt.tx.GetGattServerServicesTransaction
import io.reactivex.Single
import timber.log.Timber

/**
 * Get GATT services hosted on local GATT server
 */
class GetGattServerServices(
    private val gattServerConnection: GattServerConnection,
    private val transactionProvider: GetGattServerServicesTransactionProvider = GetGattServerServicesTransactionProvider(),
) {

    /**
     * Returns a list of GATT services offered by this device.
     */
    fun get(
        successEndState: GattState = GET_SERVER_SERVICES_SUCCESS,
    ): Single<List<BluetoothGattService>> {
        return transactionProvider.provide(gattServerConnection, successEndState)
            .runTxReactive(gattServerConnection)
            .map { result -> result.services }
            .doOnSubscribe { Timber.d("Getting GATT services for local server") }
            .doOnSuccess { Timber.d("Got GATT services for local server") }
            .doOnError { Timber.e(it, "Error Getting GATT services for local server") }
    }
}

/**
 * GetGattServerServicesTransaction provider
 */
class GetGattServerServicesTransactionProvider {

    fun provide(
        serverConnection: GattServerConnection,
        successEndState: GattState,
    ): GetGattServerServicesTransaction {
        return GetGattServerServicesTransaction(
            serverConnection,
            successEndState
        )
    }
}

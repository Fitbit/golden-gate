// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.bluetooth.fbgatt.rx.client

import com.fitbit.bluetooth.fbgatt.GattConnection
import com.fitbit.bluetooth.fbgatt.GattState.READ_RSSI_SUCCESS
import com.fitbit.bluetooth.fbgatt.rx.resetGattStateIfNecessary
import com.fitbit.bluetooth.fbgatt.rx.runTxReactive
import com.fitbit.bluetooth.fbgatt.tx.ReadRssiTransaction
import io.reactivex.Single
import timber.log.Timber

/**
 * Reactive wrapper for updating rssi value for given connection
 */
class RssiValueUpdater(
    private val readRssiTransactionProvider: (GattConnection) -> ReadRssiTransaction = { ReadRssiTransaction(it, READ_RSSI_SUCCESS) },
    private val gattClientConnectionStateSetter: GattClientConnectionStateSetter = GattClientConnectionStateSetter()
) {

    /**
     * Update Rssi value for given [gattConnection]
     *
     * This operation does not emit error instead return same [gattConnection] if updating rssi result in any errors
     *
     * @return GattConnection with updated rssi value if reading rssi is successful
     */
    fun update(gattConnection: GattConnection): Single<GattConnection> {
        return readRssiTransactionProvider(gattConnection).runTxReactive(gattConnection)
            .doOnSubscribe { Timber.d("Reading Rssi value for $gattConnection") }
            .doOnSuccess { result -> Timber.d("Reading Rssi value for ${result.rssi}") }
            .doOnError { t -> Timber.w(t, "Error reading Rssi value for $gattConnection") }
            .map { gattConnection }
            .onErrorResumeNext { t -> resetGattState(gattConnection, t) }
    }

    private fun resetGattState(gattConnection: GattConnection, t: Throwable): Single<GattConnection> {
        return gattConnection.resetGattStateIfNecessary(t, gattClientConnectionStateSetter)
            .andThen(Single.just(gattConnection))
    }
}

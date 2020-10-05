// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.bluetooth.fbgatt.rx.client

import com.fitbit.bluetooth.fbgatt.GattClientTransaction
import com.fitbit.bluetooth.fbgatt.GattConnection
import com.fitbit.bluetooth.fbgatt.GattState
import com.fitbit.bluetooth.fbgatt.GattTransaction
import com.fitbit.bluetooth.fbgatt.rx.runTxReactive
import com.fitbit.bluetooth.fbgatt.tx.SetClientConnectionStateTransaction
import io.reactivex.Completable
import io.reactivex.Single
import timber.log.Timber

/**
 * Rx wrapper for [SetClientConnectionStateTransaction] to set [GattConnection] GattState
 */
class GattClientConnectionStateSetter(
    private val transactionProvider: SetClientConnectionStateTransactionProvider = SetClientConnectionStateTransactionProvider()
) {

    /**
     * Performs GATT client connection state update
     * This assumes we are connected to the peripheral.
     *
     * @param gattConnection The GattConnection object
     * @return A [Completable] that will complete once transaction has been completed.
     */
    fun set(gattConnection: GattConnection, destinationState: GattState): Completable {
        return transactionProvider.provide(gattConnection, destinationState)
            .flatMap { transaction -> transaction.runTxReactive(gattConnection) }
            .doOnSubscribe { Timber.d("Request to set GATT connection state from ${gattConnection.gattState} to $destinationState") }
            .doOnError { t -> Timber.e(t, "Fail to set GATT connection state to $destinationState") }
            .doOnSuccess { Timber.d("Successfully set GATT connection state to $destinationState") }
            .ignoreElement()
    }
}

class SetClientConnectionStateTransactionProvider {
    fun provide(gattConnection: GattConnection, destinationState: GattState): Single<GattClientTransaction> =
        Single.fromCallable { SetClientConnectionStateTransaction(gattConnection, GattState.GATT_CONNECTION_STATE_SET_SUCCESSFULLY, destinationState) }
}

// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.bluetooth.fbgatt.rx

import android.bluetooth.BluetoothGattService
import com.fitbit.bluetooth.fbgatt.GattConnection
import com.fitbit.bluetooth.fbgatt.GattState
import com.fitbit.bluetooth.fbgatt.rx.client.GattClientConnectionStateSetter
import io.reactivex.Completable
import io.reactivex.Maybe
import io.reactivex.Single
import timber.log.Timber
import java.util.UUID

/**
 * Reactive extension function to return [BluetoothGattService] for given service uuid
 *
 * @return Single [BluetoothGattService] if remote service is found, else emmits exception
 * @throws GattServiceNotFoundException if remote service is NOT found OR peripheral is not connected
 */
fun GattConnection.getRemoteGattServiceSingle(uuid: UUID): Single<BluetoothGattService> {
    return Single.create { emitter ->
        getRemoteGattService(uuid)?.let { remoteService ->
            emitter.onSuccess(remoteService)
        } ?: emitter.onError(GattServiceNotFoundException(uuid))
    }
}

/**
 * Reactive extension function to return [BluetoothGattService] for given service uuid
 *
 * @return Maybe [BluetoothGattService] if remote service is found, else No value
 */
fun GattConnection.getRemoteGattServiceMaybe(uuid: UUID): Maybe<BluetoothGattService> =
    Maybe.fromCallable { getRemoteGattService(uuid) }

/**
 * Extension function to reset Gatt state to [GattState.IDLE] on [GattTransactionException]
 *
 * Note: This helper should only be used for transaction failures you know are not catastrophic
 * and you can recover for simply resetting them
 */
fun GattConnection.resetGattStateIfNecessary(
    t: Throwable,
    stateSetter: GattClientConnectionStateSetter = GattClientConnectionStateSetter()
): Completable {
    return Completable.defer {
        when(t) {
            is GattTransactionException -> resetGattStateIfNecessary(stateSetter)
            else ->  {
                Timber.w(t, "Ignoring Gatt reset request as its not a GattTransactionException")
                Completable.complete()
            }
        }
    }
}

/**
 * Extension function to reset Gatt state to [GattState.IDLE]
 *
 * Note: This helper should only be used for transaction failures you know are not catastrophic
 * and you can recover for simply resetting them
 */
private fun GattConnection.resetGattStateIfNecessary(
    stateSetter: GattClientConnectionStateSetter = GattClientConnectionStateSetter()
): Completable = setGattStateIfNecessary(GattState.IDLE, stateSetter)

/**
 * Extension function to set [GattConnection] GattState to [destinationState] if its not already in that state
 */
private fun GattConnection.setGattStateIfNecessary(
    destinationState: GattState,
    stateSetter: GattClientConnectionStateSetter = GattClientConnectionStateSetter()
): Completable {
    return Completable.defer {
        val currentGattState = gattState
        if(currentGattState == destinationState) {
            Timber.d("Ignoring as current Gatt state is already in state $destinationState")
            Completable.complete()
        } else {
            stateSetter.set(this, destinationState)
        }
    }
}


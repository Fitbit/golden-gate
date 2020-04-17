// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.bluetooth.fbgatt.rx.client.listeners

import com.fitbit.bluetooth.fbgatt.GattConnection
import com.fitbit.bluetooth.fbgatt.TransactionResult
import com.fitbit.bluetooth.fbgatt.rx.hexString
import io.reactivex.Emitter
import io.reactivex.Observable
import timber.log.Timber
import java.util.UUID

/**
 * GATT client listener that listens to characteristic changes on a remote device.
 * GATT characteristic changes are available per remote device
 */
class GattClientCharacteristicChangeListener {

    /**
     * Register a listener for GATT client characteristic changes
     *
     * @param gattConnection [GattConnection] for the remote peripheral to listen for characteristic changes
     * @param characteristicUuid characteristic id on remote device to listen changes for
     */
    fun register(
        gattConnection: GattConnection,
        characteristicUuid: UUID
    ): Observable<ByteArray> {
        return Observable.create<ByteArray> { emitter ->
            val listener = ClientCharacteristicChangeListener(characteristicUuid, emitter)
            gattConnection.apply {
                registerConnectionEventListener(listener)
                emitter.setCancellable {
                    unregisterConnectionEventListener(listener)
                }
            }
        }
    }

    internal class ClientCharacteristicChangeListener(
        private val characteristicUuid: UUID,
        private val emitter: Emitter<ByteArray>
    ) : BaseConnectionEventListener {
        override fun onClientCharacteristicChanged(
            result: TransactionResult,
            connection: GattConnection
        ) {
            Timber.d("onClientCharacteristicChanged from device ${connection.device.btDevice}, status: ${result.resultStatus}, data: ${result.data?.hexString()} ")
            when(result.characteristicUuid) {
                characteristicUuid -> result.data?.let { data -> emitter.onNext(data) }
                else -> Timber.d("Ignoring onClientCharacteristicChanged over ${result.characteristicUuid}")
            }
        }
    }

}

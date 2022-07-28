// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.bluetooth.fbgatt.rx.server

import android.bluetooth.BluetoothGattService
import com.fitbit.bluetooth.fbgatt.FitbitGatt
import com.fitbit.bluetooth.fbgatt.GattServerConnection
import com.fitbit.bluetooth.fbgatt.rx.GattServerNotFoundException
import com.fitbit.bluetooth.fbgatt.rx.runTxReactive
import io.reactivex.Completable
import timber.log.Timber

/**
 * Reactive API for the Bitgatt Bluetooth GATT Profile server role.
 */
class BitGattServer(
    private val fitbitGatt: FitbitGatt = FitbitGatt.getInstance(),
    private val serverTransactionProvider: ServerTransactionProvider = ServerTransactionProvider(),
    private val getGattServerServices: (serverConnection: GattServerConnection) -> GetGattServerServices = { serverConnection ->
        GetGattServerServices(serverConnection)
    },
) {

    /**
     * Returns true if up, false if not up.
     */
    fun isUp(): Boolean = fitbitGatt.server?.server != null

    /**
     * Adds a [BluetoothGattService] to this gatt server.
     * @param service The [BluetoothGattService] that will be added
     */
    fun addServices(service: BluetoothGattService): Completable {
        return fitbitGatt.server?.let { serverConnection ->
            getGattServerServices(serverConnection).get()
                .flatMapCompletable { services ->
                    if (services.map { it.uuid }.contains(service.uuid)) {
                        Timber.d("GATT service ${service.uuid} already added")
                        Completable.complete()
                    } else {
                        Timber.d("Adding GATT service ${service.uuid}")
                        serverTransactionProvider.getAddServicesTransaction(serverConnection, service)
                            .runTxReactive(serverConnection)
                            .ignoreElement()
                    }
                }
        } ?: Completable.error(GattServerNotFoundException())
    }
}

// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.bluetooth.fbgatt.rx

import com.fitbit.bluetooth.fbgatt.FitbitGatt
import com.fitbit.bluetooth.fbgatt.GattConnection
import com.fitbit.bluetooth.fbgatt.rx.client.ClientTransactionProvider
import io.reactivex.Completable
import timber.log.Timber

/**
 * Utility to disconnect a peripheral
 */
class PeripheralDisconnector(
    private val fitbitGatt: FitbitGatt = FitbitGatt.getInstance(),
    private val clientTransactionProvider: ClientTransactionProvider = ClientTransactionProvider()
) {

    /**
     * Disconnect given device if its connected
     *
     * @param bluetoothAddress BT address of Peripheral to disconnect
     */
    fun disconnect(bluetoothAddress: String): Completable {
        return fitbitGatt.getGattConnection(bluetoothAddress)
            .toSingle()
            .doOnError { Timber.w(it, "Error getting Connection for: $bluetoothAddress, may be a result of not performing BT scan with all edge cases") }
            .flatMapCompletable { gattConnection -> disconnect(gattConnection) }
    }

    /**
     * Disconnect given device if its connected
     *
     * @param connection for Peripheral to disconnect
     */
    fun disconnect(connection: GattConnection): Completable = Completable.defer {
        if (connection.isConnected) {
            val transaction = clientTransactionProvider.getDisconnectTransactionFor(connection)
            transaction.runTxReactive(connection).ignoreElement()
        } else {
            Timber.d("We were already disconnected or in the process of disconnecting, skipping disconnection")
            Completable.complete()
        }
    }
}

// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.bluetooth.fbgatt.rx.client

import android.bluetooth.BluetoothGattCharacteristic
import com.fitbit.bluetooth.fbgatt.GattClientTransaction
import com.fitbit.bluetooth.fbgatt.GattConnection
import com.fitbit.bluetooth.fbgatt.GattState
import com.fitbit.bluetooth.fbgatt.rx.GattCharacteristicException
import com.fitbit.bluetooth.fbgatt.rx.GattServiceNotFoundException
import com.fitbit.bluetooth.fbgatt.rx.dumpServicesWarning
import com.fitbit.bluetooth.fbgatt.rx.getGattCharacteristic
import com.fitbit.bluetooth.fbgatt.rx.getRemoteGattServiceSingle
import com.fitbit.bluetooth.fbgatt.rx.hexString
import com.fitbit.bluetooth.fbgatt.rx.resetGattStateIfNecessary
import com.fitbit.bluetooth.fbgatt.rx.runTxReactive
import com.fitbit.bluetooth.fbgatt.tx.ReadGattCharacteristicTransaction
import io.reactivex.Scheduler
import io.reactivex.Single
import timber.log.Timber
import java.util.UUID

/**
 * Write to remote GATT Characteristic
 *
 * @param gattConnection [GattConnection] for the remote peripheral to write to
 */
class GattCharacteristicReader constructor(
    private val gattConnection: GattConnection,
    private val readTransactionProvider: ReadGattCharacteristicTransactionProvider = ReadGattCharacteristicTransactionProvider(),
    private val gattClientConnectionStateSetter: GattClientConnectionStateSetter = GattClientConnectionStateSetter(),
    private val scheduler: Scheduler = ReadWriteCharacteristicProvider.scheduler
) {

    /**
     * Write given data to the provided remote GATT characteristic
     *
     * @param serviceId ID of the remote service to write data
     * @param characteristicId ID of the remote characteristic to write data
     * @return Data read from given GATT Characteristic
     *
     * @throws [GattServiceNotFoundException] if given remote GATT service was not found
     * @throws [GattCharacteristicException]  if given remote GATT Characteristic is not found
     */
    fun read(
        serviceId: UUID,
        characteristicId: UUID
    ): Single<ByteArray> {
        return gattConnection.getRemoteGattServiceSingle(serviceId)
            .flatMap { service -> service.getGattCharacteristic(characteristicId) }
            /**
             * we want to run all read/write operation synchronously on a dedicated single thread,
             * so GATT BluetoothGattCharacteristic value does not change before completion of that
             * transaction and we do not block any other thread waiting for completion of
             * this transaction
             */
            .observeOn(scheduler)
            .flatMap { characteristic -> read(characteristic) }
            .doOnSubscribe { Timber.d("Request to read $characteristicId characteristic") }
            .doOnSuccess { data -> Timber.d("Success reading $characteristicId characteristic data: ${data.hexString()}") }
            .doOnError { t ->
                Timber.w(t, "Failed reading $characteristicId characteristic")
                if (t is GattServiceNotFoundException) {
                    dumpServicesWarning(gattConnection.gatt?.services)
                }
            }
    }

    private fun read(gattCharacteristic: BluetoothGattCharacteristic): Single<ByteArray> {
        return readTransactionProvider.provide(gattConnection, gattCharacteristic)
            .doOnSuccess { Timber.d("Reading ${gattCharacteristic.uuid} characteristic value") }
            .flatMap { transaction: GattClientTransaction -> transaction.runTxReactive(gattConnection) }
            .onErrorResumeNext { error ->
                /**
                 * To recover from the error state of GattTransaction, we need to
                 * reset the connection state to idle, and then will still emit the read error
                 */
                gattConnection.resetGattStateIfNecessary(error, gattClientConnectionStateSetter)
                    .andThen(Single.error(error))
            }
            .map { gattCharacteristic.value }
    }

}

class ReadGattCharacteristicTransactionProvider {
    fun provide(gattConnection: GattConnection, gattCharacteristic: BluetoothGattCharacteristic) : Single<GattClientTransaction> =
        Single.fromCallable { ReadGattCharacteristicTransaction(gattConnection, GattState.READ_CHARACTERISTIC_SUCCESS, gattCharacteristic) }
}

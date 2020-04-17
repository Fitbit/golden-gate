// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.bluetooth.fbgatt.rx.client

import android.bluetooth.BluetoothGattCharacteristic
import com.fitbit.bluetooth.fbgatt.GattConnection
import com.fitbit.bluetooth.fbgatt.GattState
import com.fitbit.bluetooth.fbgatt.GattTransaction
import com.fitbit.bluetooth.fbgatt.TransactionResult
import com.fitbit.bluetooth.fbgatt.rx.GattCharacteristicException
import com.fitbit.bluetooth.fbgatt.rx.GattServiceNotFoundException
import com.fitbit.bluetooth.fbgatt.rx.getGattCharacteristic
import com.fitbit.bluetooth.fbgatt.rx.getRemoteGattServiceSingle
import com.fitbit.bluetooth.fbgatt.rx.hexString
import com.fitbit.bluetooth.fbgatt.rx.resetGattStateIfNecessary
import com.fitbit.bluetooth.fbgatt.rx.runTxReactive
import com.fitbit.bluetooth.fbgatt.tx.WriteGattCharacteristicTransaction
import io.reactivex.Completable
import io.reactivex.Scheduler
import io.reactivex.Single
import timber.log.Timber
import java.util.UUID
import java.util.concurrent.Semaphore

/**
 * Write to remote GATT Characteristic
 *
 * @param gattConnection [GattConnection] for the remote peripheral to write to
 */
class GattCharacteristicWriter constructor(
    private val gattConnection: GattConnection,
    private val writeTransactionProvider: WriteGattCharacteristicTransactionProvider = WriteGattCharacteristicTransactionProvider(),
    private val gattClientConnectionStateSetter: GattClientConnectionStateSetter = GattClientConnectionStateSetter(),
    private val scheduler: Scheduler = ReadWriteCharacteristicProvider.scheduler,
    private val readWriteLock: Semaphore = ReadWriteCharacteristicLock.lock
) {

    companion object {
        /**
         * Set this to true in order to turn on verbose logging that may impact gatt write throughput
         */
        @Volatile var slowLoggingEnabled: Boolean = false
    }

    /**
     * Write given data to the provided remote GATT characteristic
     *
     * @param serviceId ID of the remote service to write data
     * @param characteristicId ID of the remote characteristic to write data
     * @param data data to write to given GATT Characteristic with
     *
     * @throws [GattServiceNotFoundException] if given remote GATT service was not found
     * @throws [GattCharacteristicException]  if given remote GATT Characteristic is not found
     */
    fun write(
        serviceId: UUID,
        characteristicId: UUID,
        data: ByteArray
    ): Completable {
        return gattConnection.getRemoteGattServiceSingle(serviceId)
            .flatMap { service -> service.getGattCharacteristic(characteristicId) }
            /**
             * we want to run all read/write operation synchronously on a dedicated single thread,
             * so GATT BluetoothGattCharacteristic value does not change before completion of that
             * transaction and we do not block any other thread waiting for completion of
             * this transaction
             */
            .observeOn(scheduler)
            .flatMapCompletable { characteristic -> write(data, characteristic) }
            .doOnSubscribe {
                if (slowLoggingEnabled) {
                    Timber.v("Request to write $characteristicId characteristic with data: ${data.hexString()}")
                }
            }
            .doOnComplete {
                if (slowLoggingEnabled) {
                    Timber.v("Success writing to $characteristicId characteristic with data: ${data.hexString()}")
                }
            }
            .doOnError { t -> Timber.w(t, "Failed writing to $characteristicId characteristic with data: ${data.hexString()}") }
    }

    /**
     * Write given data to the provided remote GATT characteristic
     *
     * Note: This is not a thread safe operation as it modifies shared [BluetoothGattCharacteristic]
     * value and should only be executed in single thread.
     */
    private fun write(
        data: ByteArray,
        gattCharacteristic: BluetoothGattCharacteristic
    ): Completable {
        return updateGattCharacteristic(data, gattCharacteristic)
            .flatMap { characteristic ->  writeTransactionProvider.provide(gattConnection, characteristic) }
            .flatMap { transaction -> transaction.runTxReactive(gattConnection) }
            .onErrorResumeNext { t -> handleTransactionFailure(t) }
            .ignoreElement()
            .doFinally {
                readWriteLock.release()
                if (slowLoggingEnabled) {
                    Timber.v("Completed writing ${gattCharacteristic.uuid} characteristic with data: ${data.hexString()}")
                }
            }
    }

    private fun updateGattCharacteristic(
        data: ByteArray,
        gattCharacteristic: BluetoothGattCharacteristic
    ): Single<BluetoothGattCharacteristic> {
        return Single.fromCallable {
            /**
             * We want to block changing BluetoothGattCharacteristic value until transaction to write
             * to the BluetoothGattCharacteristic change is complete
             */
            if (slowLoggingEnabled) {
                Timber.v("Waiting to write ${gattCharacteristic.uuid} characteristic with data: ${data.hexString()}")
            }
            readWriteLock.acquire()
            if (slowLoggingEnabled) {
                Timber.v("Writing ${gattCharacteristic.uuid} characteristic with data: ${data.hexString()}")
            }
            gattCharacteristic.value = data
            gattCharacteristic
        }
    }

    private fun handleTransactionFailure(t: Throwable): Single<TransactionResult> {
        /**
         * To recover from the error state of GattTransaction, we need to
         * reset the connection state to idle, and then will still emit the write error
         */
        return gattConnection.resetGattStateIfNecessary(t, gattClientConnectionStateSetter)
            .andThen(Single.error(t))
    }

}

class WriteGattCharacteristicTransactionProvider {
    fun provide(gattConnection: GattConnection, gattCharacteristic: BluetoothGattCharacteristic) : Single<GattTransaction> =
        Single.fromCallable { WriteGattCharacteristicTransaction(gattConnection, GattState.WRITE_CHARACTERISTIC_SUCCESS, gattCharacteristic) }
}

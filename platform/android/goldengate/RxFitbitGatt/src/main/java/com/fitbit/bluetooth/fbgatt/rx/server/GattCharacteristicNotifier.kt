// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.bluetooth.fbgatt.rx.server

import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGattCharacteristic
import com.fitbit.bluetooth.fbgatt.FitbitBluetoothDevice
import com.fitbit.bluetooth.fbgatt.FitbitGatt
import com.fitbit.bluetooth.fbgatt.GattServerConnection
import com.fitbit.bluetooth.fbgatt.GattServerTransaction
import com.fitbit.bluetooth.fbgatt.GattState
import com.fitbit.bluetooth.fbgatt.rx.getGattCharacteristic
import com.fitbit.bluetooth.fbgatt.rx.getGattService
import com.fitbit.bluetooth.fbgatt.rx.hexString
import com.fitbit.bluetooth.fbgatt.rx.runTxReactive
import com.fitbit.bluetooth.fbgatt.tx.NotifyGattServerCharacteristicTransaction
import io.reactivex.Completable
import io.reactivex.Scheduler
import io.reactivex.Single
import io.reactivex.schedulers.Schedulers
import timber.log.Timber
import java.util.UUID
import java.util.concurrent.Executors
import java.util.concurrent.Semaphore

/**
 * Notify GATT Characteristic change
 */
class GattCharacteristicNotifier constructor(
        private val fitbitDevice: FitbitBluetoothDevice,
        private val bitgatt: FitbitGatt = FitbitGatt.getInstance(),
        private val notifyTransactionProvider: NotifyGattServerCharacteristicTransactionProvider = NotifyGattServerCharacteristicTransactionProvider(),
        private val scheduler: Scheduler = GattCharacteristicNotifierProvider.scheduler,
        private val notifierLock: Semaphore = GattCharacteristicNotifierLock.lock
) {

    constructor(device: BluetoothDevice) : this(FitbitBluetoothDevice(device))

    /**
     * Send a notification or indication that local GATT Characteristic has been updated
     *
     * @param serviceId ID of the local service to notify
     * @param characteristicId ID of the local characteristic to notify
     * @param data data to update GATT Characteristic with
     *
     * @throws [GattServerNotFoundException] if GATT server is not available
     * @throws [GattServiceNotFoundException] if GATT service was not found
     * @throws [GattCharacteristicException]  if GATT Characteristic is not found
     */
    fun notify(
            serviceId: UUID,
            characteristicId: UUID,
            data: ByteArray
    ): Completable {
        return bitgatt.server.getGattService(serviceId)
            .flatMap { service -> service.getGattCharacteristic(characteristicId) }
            /**
             * we want to run all notify operation synchronously on a dedicated single thread,
             * so BluetoothGattCharacteristic value does not change before completion of that
             * transaction and we do not block any other thread waiting for completion of
             * this transaction
             */
            .observeOn(scheduler)
            .flatMapCompletable { gattCharacteristic -> notify(data, gattCharacteristic) }
            .doOnSubscribe { Timber.d("Request to notify $characteristicId characteristic with data: ${data.hexString()}") }
            .doOnComplete { Timber.d("Success notifying $characteristicId characteristic with data: ${data.hexString()}") }
            .doOnError { Timber.w(it, "Failed to notify $characteristicId characteristic with data: ${data.hexString()}") }
    }

    /**
     * Send a notification or indication that local GATT Characteristic has been updated
     */
    private fun notify(data: ByteArray, gattCharacteristic: BluetoothGattCharacteristic): Completable {
        return updateGattCharacteristic(data, gattCharacteristic)
            .flatMap { characteristic -> notifyTransactionProvider.provide(bitgatt.server, fitbitDevice, characteristic) }
            .flatMap { gattTransaction -> gattTransaction.runTxReactive(bitgatt.server) }
            .ignoreElement()
            .doFinally {
                notifierLock.release()
                Timber.d("Completed notifying ${gattCharacteristic.uuid} characteristic with data: ${data.hexString()}")
            }
    }

    private fun updateGattCharacteristic(
        data: ByteArray,
        gattCharacteristic: BluetoothGattCharacteristic
    ): Single<BluetoothGattCharacteristic> {
        return Single.fromCallable {
            /**
             * We want to block changing GATT characteristic value until transaction to notify the
             * BluetoothGattCharacteristic change is complete
             */
            Timber.d("Waiting to notify ${gattCharacteristic.uuid} characteristic with data: ${data.hexString()}")
            notifierLock.acquire()
            Timber.d("Notifying ${gattCharacteristic.uuid} characteristic with data: ${data.hexString()}")
            gattCharacteristic.value = data
            gattCharacteristic
        }
    }

}

class NotifyGattServerCharacteristicTransactionProvider {
    fun provide(gattConnection: GattServerConnection, device: FitbitBluetoothDevice, characteristic: BluetoothGattCharacteristic): Single<GattServerTransaction> =
        Single.fromCallable { NotifyGattServerCharacteristicTransaction(gattConnection, device, GattState.NOTIFY_CHARACTERISTIC_SUCCESS, characteristic, false) }
}

/**
 * A lock to synchronize GATT Characteristic value change until the notification transaction is complete
 */
private object GattCharacteristicNotifierLock {

    val lock = Semaphore(1)

}

/**
 * Provides [Scheduler] on which all notification or indication over local GATT Characteristic are executed
 */
private object GattCharacteristicNotifierProvider {

    /**
     * Notifying local characteristic is not thread safe, so execute all transaction on single shared thread
     */
    val scheduler: Scheduler = Schedulers.from(Executors.newSingleThreadExecutor())
}

// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bt.gatt.server.services.gattlink.listeners

import android.annotation.SuppressLint
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import androidx.annotation.VisibleForTesting
import com.fitbit.bluetooth.fbgatt.FitbitBluetoothDevice
import com.fitbit.bluetooth.fbgatt.GattServerConnection
import com.fitbit.bluetooth.fbgatt.TransactionResult
import com.fitbit.bluetooth.fbgatt.rx.server.GattServerResponseSenderProvider
import com.fitbit.bluetooth.fbgatt.rx.server.listeners.BaseServerConnectionEventListener
import com.fitbit.goldengate.bindings.util.hexString
import com.fitbit.goldengate.bt.gatt.server.services.gattlink.ReceiveCharacteristic
import io.reactivex.Observable
import io.reactivex.Scheduler
import io.reactivex.schedulers.Schedulers
import io.reactivex.subjects.BehaviorSubject
import io.reactivex.subjects.Subject
import timber.log.Timber

/**
 * GattLink service listener for listening for any data received on its Rx characteristic.
 *
 * This listener will receive data from all connected peripherals and it is expected that
 * a device specific listener has already registered itself to receive the data. If there
 * is no device specific request handler registered an error response will be sent back
 * to the peripheral
 *
 * **Singleton** as we only want one listener Rx characteristic write request
 */
internal class ReceiveCharacteristicDataListener @VisibleForTesting constructor(
        private val responseScheduler: Scheduler = Schedulers.io(),
        private val gattServerResponseSenderProvider: GattServerResponseSenderProvider = GattServerResponseSenderProvider()
) : BaseServerConnectionEventListener {

    private object SingletonHolder {
        val INSTANCE = ReceiveCharacteristicDataListener()
    }

    companion object {
        val instance: ReceiveCharacteristicDataListener by lazy { SingletonHolder.INSTANCE }
    }

    private val registry = hashMapOf<BluetoothDevice, Subject<ByteArray>>()

    /**
     * Handler capable of receiving data on GattLink Rx characteristics should register itself
     *
     * @param device for which receiver is to be registered
     * @return Data received (over Rx characteristics) will be available on this Observable
     */
    @Synchronized
    fun register(device: BluetoothDevice): Observable<ByteArray> {
        Timber.d("Registering Rx receiver for ${device.address}")
        val dataSubject = BehaviorSubject.create<ByteArray>()
        registry[device] = dataSubject
        return dataSubject
    }

    /**
     * Unregister a previously registered receiver
     *
     * @param device for which receiver is to be unregistered
     */
    @Synchronized
    fun unregister(device: BluetoothDevice) {
        Timber.d("Unregistering Rx receiver for ${device.address}")
        registry.remove(device)
    }

    @VisibleForTesting
    @Synchronized
    fun unregisterAll() {
        registry.clear()
    }

    override fun onServerCharacteristicWriteRequest(
            device: BluetoothDevice,
            result: TransactionResult,
            connection: GattServerConnection
    ) {
        when (result.characteristicUuid) {
            ReceiveCharacteristic.uuid -> receiveData(device, result, connection)
            else -> Timber.d("Ignoring as its not a request for characteristic we listening to, requestId: ${result.requestId} on uuid: ${result.characteristicUuid}")
        }
    }

    @Synchronized
    private fun receiveData(
            device: BluetoothDevice,
            result: TransactionResult,
            connection: GattServerConnection
    ) {
        Timber.d("received data on Rx characteristic: ${result.data?.hexString()}")
        result.data?.let { data ->
            registry[device]?.let { dataObservable ->
                dataObservable.onNext(data)
                if (result.isResponseRequired) {
                    // send SUCCESS response if there is a registered listener
                    sendResponse(device, connection, result.requestId, BluetoothGatt.GATT_SUCCESS)
                }
            } ?: handleNoReceiverRegistered(device, result, connection)
        } ?: handlerNullDataReceived(device, result, connection)

    }

    private fun handleNoReceiverRegistered(
            device: BluetoothDevice,
            result: TransactionResult,
            connection: GattServerConnection
    ) {
        Timber.w("Cannot process requestId: ${result.requestId} on uuid: ${ReceiveCharacteristic.uuid} as no data listener is registered")
        if (result.isResponseRequired) {
            // send ERROR response if not listener is registered to process the incoming data
            sendResponse(device, connection, result.requestId, BluetoothGatt.GATT_FAILURE)
        }
    }

    private fun handlerNullDataReceived(
            device: BluetoothDevice,
            result: TransactionResult,
            connection: GattServerConnection
    ) {
        Timber.d("Ignoring requestId: ${result.requestId} on uuid: ${ReceiveCharacteristic.uuid} as data received is null")
        if (result.isResponseRequired) {
            // By default we do nothing when null data is received and just send success response
            sendResponse(device, connection, result.requestId, BluetoothGatt.GATT_SUCCESS)
        }
    }

    @SuppressLint("CheckResult", "RxLeakedSubscription")
    private fun sendResponse(
            device: BluetoothDevice,
            connection: GattServerConnection,
            requestId: Int,
            status: Int
    ) {
        Timber.d("Sending response with status: $status for device: ${device.address}")
        gattServerResponseSenderProvider.provide(connection).send(
                device = FitbitBluetoothDevice(device),
                requestId = requestId,
                status = status
        )
                .subscribeOn(responseScheduler)
                .subscribe(
                        { Timber.d("Successfully sent $status response for Gatt server write request") },
                        { Timber.e(it, "Error sending $status response for Gatt server write request") }
                )
    }

}

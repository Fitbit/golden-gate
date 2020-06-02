package com.fitbit.goldengate.bt.gatt.server.services.gattcache

import android.annotation.SuppressLint
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import com.fitbit.bluetooth.fbgatt.FitbitBluetoothDevice
import com.fitbit.bluetooth.fbgatt.GattServerConnection
import com.fitbit.bluetooth.fbgatt.TransactionResult
import com.fitbit.bluetooth.fbgatt.rx.server.GattServerResponseSenderProvider
import com.fitbit.bluetooth.fbgatt.rx.server.listeners.BaseServerConnectionEventListener
import com.fitbit.goldengate.bindings.util.hexString
import com.fitbit.goldengate.bt.gatt.util.toByteArray
import io.reactivex.Scheduler
import io.reactivex.schedulers.Schedulers
import timber.log.Timber

class GattCacheServiceEventListener internal constructor(
    private val responseScheduler: Scheduler = Schedulers.io(),
    private val gattServerResponseSenderProvider: GattServerResponseSenderProvider = GattServerResponseSenderProvider()
) : BaseServerConnectionEventListener {

    override fun onServerCharacteristicReadRequest(
        device: BluetoothDevice,
        result: TransactionResult,
        connection: GattServerConnection
    ) {
        Timber.d(
            """
            Handle onServerCharacteristicReadRequest call from
            device ${device.address},
            service: ${result.serviceUuid},
            characteristicUuid: ${result.characteristicUuid},
            descriptorUuid: ${result.descriptorUuid}
            """
        )

        when (result.serviceUuid) {
            GattCacheService.uuid -> handleGattCacheServiceCharacteristicReadRequest(device, result, connection)
            else -> Timber.d("Ignoring onServerCharacteristicReadRequest call for unsupported service: ${result.serviceUuid}")
        }
    }

    override fun onServerCharacteristicWriteRequest(
        device: BluetoothDevice,
        result: TransactionResult,
        connection: GattServerConnection
    ) {
        Timber.d(
            """
            Handle onServerCharacteristicWriteRequest call from
            device ${device.address},
            service: ${result.serviceUuid},
            characteristicUuid: ${result.characteristicUuid},
            descriptorUuid: ${result.descriptorUuid}
            """
        )
        when (result.serviceUuid) {
            GattCacheService.uuid -> handleGattCacheServiceCharacteristicWriteRequest(device, result, connection)
            else -> Timber.d("Ignoring as its not a request for characteristic we listening to, requestId: ${result.requestId} on uuid: ${result.characteristicUuid}")
        }
    }

    private fun handleGattCacheServiceCharacteristicReadRequest(
        device: BluetoothDevice,
        result: TransactionResult,
        connection: GattServerConnection
    ) {
        when (result.characteristicUuid) {
            EphemeralCharacteristicPointer.uuid -> sendResponse(
                device,
                connection,
                result.requestId,
                BluetoothGatt.GATT_SUCCESS,
                EphemeralCharacteristic.uuid.toByteArray()
            )
            else -> {
                Timber.d("Ignoring onServerDescriptorWriteRequest call for unsupported characteristicUuid: ${result.characteristicUuid}")
                if (result.isResponseRequired) {
                    sendFailureResponseIfRequested(device, result, connection)
                }
            }
        }
    }

    private fun handleGattCacheServiceCharacteristicWriteRequest(
        device: BluetoothDevice,
        result: TransactionResult,
        connection: GattServerConnection
    ) {
        when (result.characteristicUuid) {
            EphemeralCharacteristic.uuid -> receiveData(device, result, connection)
            else -> {
                Timber.d("Ignoring onServerDescriptorWriteRequest call for unsupported characteristicUuid: ${result.characteristicUuid}")
                if (result.isResponseRequired) {
                    sendFailureResponseIfRequested(device, result, connection)
                }
            }
        }
    }

    private fun sendFailureResponseIfRequested(
        device: BluetoothDevice,
        result: TransactionResult,
        connection: GattServerConnection
    ) {
        if (result.isResponseRequired) {
            // By default we do nothing when null data is received and just send success response
            sendResponse(device, connection, result.requestId, BluetoothGatt.GATT_FAILURE)
        }
    }

    @SuppressLint("CheckResult", "RxLeakedSubscription")
    private fun sendResponse(
        device: BluetoothDevice,
        connection: GattServerConnection,
        requestId: Int,
        status: Int,
        value: ByteArray? = null
    ) {
        Timber.d("Sending response with status: $status for device: ${device.address} with data: $value")
        gattServerResponseSenderProvider.provide(connection).send(
            device = FitbitBluetoothDevice(device),
            requestId = requestId,
            status = status,
            value = value
        )
            .subscribeOn(responseScheduler)
            .subscribe(
                { Timber.d("Successfully sent $status response for Gatt server write request") },
                { Timber.e(it, "Error sending $status response for Gatt server write request") }
            )
    }

    @Synchronized
    private fun receiveData(
        device: BluetoothDevice,
        result: TransactionResult,
        connection: GattServerConnection
    ) {
        Timber.d("received data on Ephemeral Characteristic: ${result.data?.hexString()}")
        if (result.isResponseRequired) {
            val data = result.data
            if (data != null && data[0] == 0.toByte()) {
                sendResponse(device, connection, result.requestId, BluetoothGatt.GATT_SUCCESS)
            } else {
                sendResponse(device, connection, result.requestId, BluetoothGatt.GATT_FAILURE)
            }
        }
    }
}

// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.bluetooth.fbgatt.rx.client

import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattDescriptor
import android.bluetooth.BluetoothGattService
import com.fitbit.bluetooth.fbgatt.FitbitBluetoothDevice
import com.fitbit.bluetooth.fbgatt.GattConnection
import com.fitbit.bluetooth.fbgatt.rx.CLIENT_CONFIG_UUID
import com.fitbit.bluetooth.fbgatt.rx.runTxReactive
import com.fitbit.bluetooth.fbgatt.tx.RequestGattConnectionIntervalTransaction
import io.reactivex.Maybe
import io.reactivex.Notification
import io.reactivex.Single
import timber.log.Timber
import java.util.UUID

/**
 * Duplicating [BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE], [BluetoothGattDescriptor.ENABLE_INDICATION_VALUE]
 * and [BluetoothGattDescriptor.DISABLE_NOTIFICATION_VALUE]
 * values as a work around for exception thrown when accessing these value directly from unit test
 */
internal val gattEnableNotificationValue = byteArrayOf(0x01, 0x00)
internal val gattEnableIndicationValue = byteArrayOf(0x02, 0x00)
internal val gattDisableNotificationValue = byteArrayOf(0x00, 0x00)

/**
 * Interface representing a BLE peripheral and operations that can be carried on it.
 */
class BitGattPeripheral(
    val gattConnection: GattConnection,
    private val clientTransactionProvider: ClientTransactionProvider = ClientTransactionProvider()
) {

    /**
     * Establishes a BLE connection with a peripheral. Returns a [BluetoothGatt] object.
     * @return A [Single] that will emit the [BluetoothGatt] instance associated with this peripheral.
     */
    fun connect(): Single<BluetoothGatt> = Single.defer {
        if (gattConnection.isConnected) {
            Timber.d("We are already connected, just returning the gatt")
            Single.just(gattConnection.gatt)
        } else {
            Timber.d("Connecting...")
            val transaction = clientTransactionProvider.getConnectTransactionFor(gattConnection)
            transaction.runTxReactive(gattConnection)
                .map { gattConnection.gatt }
        }
    }

    /**
     * Requests connection priority as described in the [BluetoothGatt] object.
     * This assumes we are connected to the peripheral.
     * @param priority Connection priority [BluetoothGatt.CONNECTION_PRIORITY_LOW_POWER], [BluetoothGatt.CONNECTION_PRIORITY_BALANCED] or [BluetoothGatt.CONNECTION_PRIORITY_HIGH].
     * @return A [Single] emitting a boolean depending with the result of the boolean (true -> success, false -> failure).
     */
    fun requestConnectionPriority(priority: Int): Single<Boolean> {
        Timber.d("Requesting connection priority...")
        val speed = when (priority) {
            BluetoothGatt.CONNECTION_PRIORITY_HIGH -> RequestGattConnectionIntervalTransaction.Speed.HIGH
            BluetoothGatt.CONNECTION_PRIORITY_BALANCED -> RequestGattConnectionIntervalTransaction.Speed.MID
            BluetoothGatt.CONNECTION_PRIORITY_LOW_POWER -> RequestGattConnectionIntervalTransaction.Speed.LOW
            else -> {
                Timber.w("Unsupported connection priority value : $priority")
                RequestGattConnectionIntervalTransaction.Speed.MID
            }
        }
        return Single.defer {
            val transaction = clientTransactionProvider.getRequestConnectionIntervalTransactionFor(
                gattConnection,
                speed
            )

            transaction.runTxReactive(gattConnection)
                .map { true }
        }
    }

    /**
     * Requests an MTU
     * This assumes we are connected to the peripheral.
     * @param mtu Request an MTU size for the connection
     * @return A [Single] emitting an integer containing the MTU size
     */
    fun requestMtu(mtu: Int): Single<Int> = Single.defer {
        Timber.d("Requesting MTU of $mtu")
        val transaction = clientTransactionProvider.getRequestMtuTransactionFor(gattConnection, mtu)
        transaction.runTxReactive(gattConnection)
            .map { result -> result.mtu }
    }

    /**
     * Performs service discovery on the peripheral
     * This assumes we are connected to the peripheral.
     * @return A [Single] containing a [List] of discovered [BluetoothGattService]s
     */
    fun discoverServices(): Single<List<BluetoothGattService>> = Single.defer {
        Timber.d("Discovering services...")
        val transaction =
            clientTransactionProvider.getDiscoverServicesTransactionFor(gattConnection)
        transaction.runTxReactive(gattConnection)
            .map { gattConnection.gatt?.services ?: emptyList() }
    }

    /**
     * Subscribe to a characteristic
     * This assumes we are connected to the peripheral.
     * @param characteristic The [BluetoothGattCharacteristic] from which the value should be read
     * @return A [Single] containing the [BluetoothGattCharacteristic] and its value
     */
    private fun subscribeCharacteristic(
        characteristic: BluetoothGattCharacteristic
    ): Single<BluetoothGattCharacteristic> = Single.defer {
        val transaction = clientTransactionProvider.getSubscribeToGattCharacteristicTransactionFor(
            gattConnection,
            characteristic
        )
        transaction.runTxReactive(gattConnection)
            .map { characteristic }
    }

    /**
     * Unsubscribe to a characteristic
     * This assumes we are connected to the peripheral.
     * @param characteristic The [BluetoothGattCharacteristic] from which the value should be read
     * @return A [Single] containing the [BluetoothGattCharacteristic] and its value
     */
    private fun unsubscribeCharacteristic(
        characteristic: BluetoothGattCharacteristic
    ): Single<BluetoothGattCharacteristic> = Single.defer {
        val transaction = clientTransactionProvider.getUnsubscribeFromGattCharacteristicTransactionFor(
            gattConnection,
            characteristic
        )
        transaction.runTxReactive(gattConnection)
            .map { characteristic }
    }

    /**
     * Write to a descriptor characteristic
     * This assumes we are connected to the peripheral.
     * @param characteristic The [BluetoothGattCharacteristic] from which the value should be read
     * @param value The value to write to the descriptor characteristic
     * @return A [Single] containing the [BluetoothGattCharacteristic] and its value
     */
    private fun writeDescriptorCharacteristic(
        characteristic: BluetoothGattCharacteristic,
        value: ByteArray
    ): Single<BluetoothGattCharacteristic> = Single.defer {
        val descriptor = characteristic.getDescriptor(CLIENT_CONFIG_UUID)
        descriptor.value = value

        val transaction =
            clientTransactionProvider.getWriteDescriptorTransaction(gattConnection, descriptor)
        transaction.runTxReactive(gattConnection)
            .map { characteristic }
    }

    /**
     * Combined operation to set up notifications on a characteristic (enables notifications + writes to descriptor
     * This assumes we are connected to the peripheral.
     * @param characteristic The [BluetoothGattCharacteristic] on which to enable notifications.
     * @param subscribe true/false, whether we are subscribing from the characteristic or unsubscribing
     * @param isIndication true/false, whether the notification type is indication or notification
     * @return A [Single] containing the [BluetoothGattCharacteristic] on which notifications were setup/disabled
     */
    fun setupNotifications(
        characteristic: BluetoothGattCharacteristic,
        subscribe: Boolean,
        isIndication: Boolean = false
    ): Single<BluetoothGattCharacteristic> = Single.defer {
        if(subscribe) {
            subscribeCharacteristic(characteristic)
                .flatMap { writeDescriptorCharacteristic(characteristic, if (isIndication) gattEnableIndicationValue else gattEnableNotificationValue) }
                .doOnError { Timber.w(it, "fail to subscribe notification...") }
        } else {
            unsubscribeCharacteristic(characteristic)
                .flatMap { writeDescriptorCharacteristic(characteristic, gattDisableNotificationValue) }
                .doOnError { Timber.w(it, "fail to unsubscribe notification...") }
        }
    }

    /**
     * Reads the descriptor of a characteristic
     * This assumes we are connected to the peripheral.
     * @param descriptor The [BluetoothGattDescriptor] from which the value will be read
     * @return A [Single] containing the [BluetoothGattDescriptor] and its value
     */
    fun readDescriptor(descriptor: BluetoothGattDescriptor): Single<BluetoothGattDescriptor> = Single.defer {
        val transaction =
            clientTransactionProvider.getReadDescriptorTransactionFor(gattConnection, descriptor)
        transaction.runTxReactive(gattConnection)
            .map { descriptor }
    }

    /**
     * Gets the service matching a specific UUID. Prior to this service discovery needs to be performed.
     * This assumes we are connected to the peripheral.
     * @param uuid The [UUID] of the [BluetoothGattService] to get.
     * @return A [Maybe] containing [BluetoothGattService] matching the uuid. Empty if not found.
     */
    fun getService(uuid: UUID): Maybe<BluetoothGattService> {
        return Maybe.fromCallable { gattConnection.gatt?.getService(uuid) }
    }

    /**
     * Gets the list of all services on the peripheral. Prior to this service discovery needs to be performed.
     * This assumes we are connected to the peripheral.
     * @return A [List] of [BluetoothGattService].
     */
    val services: List<BluetoothGattService>?
        get() = gattConnection.gatt?.services

    /**
     * Gets the fitbitDevice associated with the peripheral
     * This assumes we are connected to the peripheral.
     * @return The [FitbitBluetoothDevice] associated with it.
     */
    val fitbitDevice: FitbitBluetoothDevice
        get() = gattConnection.device

    /**
     * Whether or not the [GattConnection] is [connected][GattConnection.isConnected]
     * @return **true** if the [GattConnection] is connected or **false** otherwise.
     */
    val isConnected: Boolean
        get() = gattConnection.isConnected
}

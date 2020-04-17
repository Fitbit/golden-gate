// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.bluetooth.fbgatt.rx.client

import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattDescriptor
import com.fitbit.bluetooth.fbgatt.GattConnection
import com.fitbit.bluetooth.fbgatt.GattState
import com.fitbit.bluetooth.fbgatt.GattTransaction
import com.fitbit.bluetooth.fbgatt.tx.GattClientDiscoverServicesTransaction
import com.fitbit.bluetooth.fbgatt.tx.GattConnectTransaction
import com.fitbit.bluetooth.fbgatt.tx.GattDisconnectTransaction
import com.fitbit.bluetooth.fbgatt.tx.ReadGattDescriptorTransaction
import com.fitbit.bluetooth.fbgatt.tx.RequestGattConnectionIntervalTransaction
import com.fitbit.bluetooth.fbgatt.tx.RequestMtuGattTransaction
import com.fitbit.bluetooth.fbgatt.tx.SubscribeToCharacteristicNotificationsTransaction
import com.fitbit.bluetooth.fbgatt.tx.UnSubscribeToGattCharacteristicNotificationsTransaction
import com.fitbit.bluetooth.fbgatt.tx.WriteGattDescriptorTransaction

/**
 * Implementers of this interface can provide alternative implementations of the bitgatt transactions, like mocking in tests.
 */
class ClientTransactionProvider {

    /**
     * Provides a bitgatt connect transaction
     * @param gattConnection The connection on which the transaction will run.
     */
    fun getConnectTransactionFor(gattConnection: GattConnection): GattTransaction
        = GattConnectTransaction(gattConnection, GattState.CONNECTED)

    /**
     * Provides a bitgatt disconnect transaction
     * @param gattConnection The connection on which the transaction will run.
     */
    fun getDisconnectTransactionFor(gattConnection: GattConnection): GattTransaction
        = GattDisconnectTransaction(gattConnection, GattState.DISCONNECTED)

    /**
     * Provides a bitgatt request connection interval transaction
     * @param gattConnection The connection on which the transaction will run.
     * @param speed The connection priority
     */
    fun getRequestConnectionIntervalTransactionFor(gattConnection: GattConnection, speed: RequestGattConnectionIntervalTransaction.Speed): GattTransaction
        = RequestGattConnectionIntervalTransaction(gattConnection, GattState.REQUEST_CONNECTION_INTERVAL_SUCCESS, speed)

    /**
     * Provides a bitgatt request mtu transaction
     * @param gattConnection The connection on which the transaction will run.
     * @param mtu the MTU
     */
    fun getRequestMtuTransactionFor(gattConnection: GattConnection, mtu: Int): GattTransaction
        = RequestMtuGattTransaction(gattConnection, GattState.REQUEST_MTU_SUCCESS, mtu)

    /**
     * Provides a bitgatt discover services transaction
     * @param gattConnection The connection on which the transaction will run.
     */
    fun getDiscoverServicesTransactionFor(gattConnection: GattConnection): GattTransaction
        = GattClientDiscoverServicesTransaction(gattConnection, GattState.DISCOVERY_SUCCESS)

    /**
     * Provides a bitgatt read descriptor transaction
     * @param gattConnection The connection on which the transaction will run.
     */
    fun getReadDescriptorTransactionFor(gattConnection: GattConnection, descriptor: BluetoothGattDescriptor): GattTransaction
        = ReadGattDescriptorTransaction(gattConnection, GattState.READ_DESCRIPTOR_SUCCESS, descriptor)

    /**
     * Provides a subscribe to gatt characteristic bitgatt transaction. This operation does not run any bluetooth specific operation,
     * instead just tells the system our app is interested in notifications from this characteristic.
     * @param gattConnection The connection on which the transaction will run.
     * @param bluetoothGattCharacteristic The characteristic on which we want to receive notifications.
     */
    fun getSubscribeToGattCharacteristicTransactionFor(gattConnection: GattConnection, bluetoothGattCharacteristic: BluetoothGattCharacteristic): GattTransaction
        = SubscribeToCharacteristicNotificationsTransaction(gattConnection, GattState.ENABLE_CHARACTERISTIC_NOTIFICATION_SUCCESS, bluetoothGattCharacteristic)

    /**
     * Provides an unsubscribe to gatt characteristic bitgatt transaction. This operation does not run any bluetooth specific operation,
     * instead just tells the system our app is not interested in notifications from this characteristic anymore.
     * @param gattConnection The connection on which the transaction will run.
     * @param bluetoothGattCharacteristic The characteristic for which we want to unsubscribe.
     */
    fun getUnsubscribeFromGattCharacteristicTransactionFor(gattConnection: GattConnection, bluetoothGattCharacteristic: BluetoothGattCharacteristic):GattTransaction
        = UnSubscribeToGattCharacteristicNotificationsTransaction(gattConnection, GattState.DISABLE_CHARACTERISTIC_NOTIFICATION_SUCCESS, bluetoothGattCharacteristic)

    /**
     * Provides a write descriptor bitgatt transaction.
     * @param gattConnection The connection on which the transaction will run.
     * @param descriptor The descriptor with [descriptor.value] we want to write to it.
     */
    fun getWriteDescriptorTransaction(gattConnection: GattConnection, descriptor: BluetoothGattDescriptor): GattTransaction
        = WriteGattDescriptorTransaction(gattConnection, GattState.WRITE_DESCRIPTOR_SUCCESS, descriptor)

}

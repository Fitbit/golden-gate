// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bt.gatt

import com.fitbit.bluetooth.fbgatt.GattServerConnection
import com.fitbit.bluetooth.fbgatt.rx.server.listeners.GattServerConnectionChangeListenerForPeripheralMode
import com.fitbit.bluetooth.fbgatt.rx.server.listeners.GattServerConnectionChangeListener
import com.fitbit.goldengate.bt.gatt.server.services.gattlink.listeners.ReceiveCharacteristicDataListener
import com.fitbit.goldengate.bt.gatt.server.services.gattlink.listeners.TransmitCharacteristicSubscriptionListener
import io.reactivex.Completable

/**
 * Helper to register GATT listeners
 */
object GattServerListenerRegistrar {

    /**
     * Register all global GATT server listeners
     *
     * @param serverConnection local GATT Server connection
     */
    fun registerGattServerListeners(serverConnection: GattServerConnection): Completable {
        return Completable.fromCallable {
            serverConnection.registerConnectionEventListener(GattServerConnectionChangeListener)
        }
    }

    fun registerGattServerNodeListeners(serverConnection: GattServerConnection): Completable {
        return Completable.fromCallable {
            serverConnection.registerConnectionEventListener(GattServerConnectionChangeListenerForPeripheralMode)
            serverConnection.registerConnectionEventListener(TransmitCharacteristicSubscriptionListener.instance)
            serverConnection.registerConnectionEventListener(ReceiveCharacteristicDataListener.instance)
        }
    }
}

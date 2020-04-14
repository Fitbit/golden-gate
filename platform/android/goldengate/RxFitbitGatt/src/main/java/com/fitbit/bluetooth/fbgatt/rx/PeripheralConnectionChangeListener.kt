package com.fitbit.bluetooth.fbgatt.rx

import com.fitbit.bluetooth.fbgatt.GattConnection
import com.fitbit.bluetooth.fbgatt.rx.client.listeners.GattClientConnectionChangeListener
import com.fitbit.bluetooth.fbgatt.rx.server.listeners.GattServerConnectionChangeListener
import io.reactivex.Observable

/**
 * Listener to observer peripheral connection state changes
 */
class PeripheralConnectionChangeListener(
        private val serverConnectionChangeListener: GattServerConnectionChangeListener = GattServerConnectionChangeListener,
        private val clientConnectionChangeListener: GattClientConnectionChangeListener = GattClientConnectionChangeListener()
) {

    /**
     * Register a listener for connection state changes for a device
     *
     * @return will emit connection state changes if GATT client or server disconnect is detected
     */
    fun register(gattConnection: GattConnection): Observable<PeripheralConnectionStatus> {
        return serverConnectionChangeListener.getDataObservable(gattConnection.device.btDevice)
                .mergeWith(clientConnectionChangeListener.register(gattConnection))
                .distinctUntilChanged()
    }
}

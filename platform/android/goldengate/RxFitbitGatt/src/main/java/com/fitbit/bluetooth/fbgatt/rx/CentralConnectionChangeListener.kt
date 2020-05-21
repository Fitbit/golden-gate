package com.fitbit.bluetooth.fbgatt.rx

import com.fitbit.bluetooth.fbgatt.rx.server.listeners.GattServerConnectionChangeListenerForPeripheralMode
import com.fitbit.bluetooth.fbgatt.rx.server.listeners.ConnectionEvent
import io.reactivex.Observable

class CentralConnectionChangeListener (
    private val serverConnectionChangeListener: GattServerConnectionChangeListenerForPeripheralMode = GattServerConnectionChangeListenerForPeripheralMode
) {

    /**
     * Register a listener for connection state changes for a device
     *
     * @return will emit connection state changes if GATT client or server disconnect is detected
     */
    fun register(): Observable<ConnectionEvent> {
        return serverConnectionChangeListener.getDataObservable()
            .distinctUntilChanged()
    }
}

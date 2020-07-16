package com.fitbit.bluetooth.fbgatt.rx

import com.fitbit.bluetooth.fbgatt.GattConnection
import com.fitbit.bluetooth.fbgatt.rx.client.listeners.GattClientMtuChangeListener
import com.fitbit.bluetooth.fbgatt.rx.server.listeners.GattServerMtuChangeListener
import io.reactivex.Observable

/**
 * Listener to observe MTU changes from [BitGatt] callbacks
 */
class MtuUpdateListener(
    private val serverMtuChangeListener: GattServerMtuChangeListener = GattServerMtuChangeListener,
    private val clientMtuChangeListener: GattClientMtuChangeListener = GattClientMtuChangeListener()
) {
    /**
     * Register a listener for MTU size update
     *
     * @return will emit new MTU size from onMtuChanged callbacks
     */
    fun register(
        gattConnection: GattConnection
    ): Observable<Int> {
        return serverMtuChangeListener.getMtuSizeObservable(gattConnection.device.btDevice)
            .mergeWith(clientMtuChangeListener.register(gattConnection))
            .distinctUntilChanged()
    }
}

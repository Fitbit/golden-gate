package com.fitbit.bluetooth.fbgatt.rx.client.listeners

import com.fitbit.bluetooth.fbgatt.GattConnection
import com.fitbit.bluetooth.fbgatt.TransactionResult
import com.fitbit.bluetooth.fbgatt.TransactionResult.TransactionResultStatus.SUCCESS
import com.fitbit.bluetooth.fbgatt.rx.PeripheralConnectionStatus
import io.reactivex.Emitter
import io.reactivex.Observable
import timber.log.Timber

/**
 * GATT client listener that listens to connection changes with a remote device.
 * Connection changes are available per remote device
 */
class GattClientConnectionChangeListener {

    /**
     * Register a listener for GATT client connection changes
     */
    fun register(gattConnection: GattConnection): Observable<PeripheralConnectionStatus> {
        return Observable.create<PeripheralConnectionStatus> { emitter ->
            val listener = ClientConnectionChangeListener(emitter)
            gattConnection.apply {
                registerConnectionEventListener(listener)
                emitter.setCancellable {
                    unregisterConnectionEventListener(listener)
                }
            }
        }
    }

    internal class ClientConnectionChangeListener(private val emitter: Emitter<PeripheralConnectionStatus>) :
        BaseConnectionEventListener {
        override fun onClientConnectionStateChanged(
            result: TransactionResult,
            connection: GattConnection
        ) {
            Timber.d("onClientConnectionStateChanged from device ${connection.device.btDevice}, status: ${result.resultStatus}")
            when (result.resultStatus) {
                SUCCESS -> emitter.onNext(PeripheralConnectionStatus.CONNECTED)
                else -> emitter.onNext(PeripheralConnectionStatus.DISCONNECTED)
            }
        }
    }

}

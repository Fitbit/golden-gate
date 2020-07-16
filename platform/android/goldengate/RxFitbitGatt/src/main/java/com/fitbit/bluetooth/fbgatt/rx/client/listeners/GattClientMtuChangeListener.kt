package com.fitbit.bluetooth.fbgatt.rx.client.listeners

import com.fitbit.bluetooth.fbgatt.GattConnection
import com.fitbit.bluetooth.fbgatt.TransactionResult
import com.fitbit.bluetooth.fbgatt.TransactionResult.TransactionResultStatus.SUCCESS
import io.reactivex.Emitter
import io.reactivex.Observable
import timber.log.Timber

/**
 * GATT client listener that listens to MTU changes with a remote device.
 * Connection changes are available per remote device
 */
class GattClientMtuChangeListener {
    /**
     * Register a listener for GATT client MTU update
     */
    fun register(gattConnection: GattConnection): Observable<Int> {
        return Observable.create<Int> { emitter ->
            val listener = ClientMtuChangeListener(emitter)
            gattConnection.apply {
                registerConnectionEventListener(listener)
                emitter.setCancellable {
                    unregisterConnectionEventListener(listener)
                }
            }
        }
    }

    internal class ClientMtuChangeListener(private val emitter: Emitter<Int>) :
        BaseConnectionEventListener {

        override fun onMtuChanged(
            result: TransactionResult,
            connection: GattConnection
        ) {
            Timber.d("onClientMtuChanged from device ${connection.device.btDevice}, status: ${result.resultStatus}, mtu: ${result.mtu}")
            when (result.resultStatus) {
                SUCCESS -> emitter.onNext(result.mtu)
                else -> Timber.w("onMtuChanged transaction failed for ${connection.device.btDevice}")
            }
        }
    }
}


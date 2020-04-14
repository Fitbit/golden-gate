package com.fitbit.bluetooth.fbgatt.rx.server.listeners

import android.bluetooth.BluetoothDevice
import com.fitbit.bluetooth.fbgatt.FitbitGatt
import com.fitbit.bluetooth.fbgatt.GattServerConnection
import com.fitbit.bluetooth.fbgatt.TransactionResult
import com.fitbit.bluetooth.fbgatt.TransactionResult.TransactionResultStatus
import io.reactivex.Emitter
import io.reactivex.Observable
import timber.log.Timber

/**
 * GATT server listener that listens to MTU changes with a remote device.
 */
class GattServerMtuChangeListener(
    private val fitbitGatt: FitbitGatt = FitbitGatt.getInstance()
) {

    /**
     * Listen to GATT server MTU changes
     *
     * @param device connected device for which MTU changes result will be available
     */
    fun register(device: BluetoothDevice): Observable<Int> {
        return Observable.create<Int> { emitter ->
            val listener = ServerMtuChangeListener(device, emitter)
            fitbitGatt.server.apply {
                registerConnectionEventListener(listener)
                emitter.setCancellable {
                    unregisterConnectionEventListener(listener)
                }
            }
        }
    }

    internal class ServerMtuChangeListener(
        private val device: BluetoothDevice,
        private val emitter: Emitter<Int>
    ) : BaseServerConnectionEventListener {

        override fun onServerMtuChanged(
            device: BluetoothDevice,
            result: TransactionResult,
            connection: GattServerConnection
        ) {
            val registeredDevice = device == this.device
            Timber.d("Got onServerMtuChanged call from device ${device.address}, registered: $registeredDevice, result: $result")
            if(registeredDevice) {
                when (result.resultStatus) {
                    TransactionResultStatus.SUCCESS -> emitter.onNext(result.mtu)
                    else -> Timber.w("onMtuChanged transaction failed for $device")
                }
            }
        }
    }

}

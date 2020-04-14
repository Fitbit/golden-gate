package com.fitbit.bluetooth.fbgatt.rx.server.listeners

import android.bluetooth.BluetoothDevice
import com.fitbit.bluetooth.fbgatt.GattServerConnection
import com.fitbit.bluetooth.fbgatt.TransactionResult
import com.fitbit.bluetooth.fbgatt.TransactionResult.TransactionResultStatus
import com.fitbit.bluetooth.fbgatt.rx.PeripheralConnectionStatus
import io.reactivex.Observable
import io.reactivex.subjects.BehaviorSubject
import io.reactivex.subjects.Subject
import timber.log.Timber

/**
 * GATT server listener that listens to connection changes with a remote device.
 * Connection changes are available per remote device
 *
 * Note: This Singleton listener must be registered when app in initialized
 */
object GattServerConnectionChangeListener : BaseServerConnectionEventListener {

    private val registry = hashMapOf<BluetoothDevice, Subject<PeripheralConnectionStatus>>()

    /**
     * Observable on which changes to Gatt server connection state changes are available
     *
     * @param device remote device for which connection state change need to be observer
     * @return observable to broadcast Gatt server connection state changes per device
     */
    fun getDataObservable(device: BluetoothDevice): Observable<PeripheralConnectionStatus> =
            getDataSubject(device)

    override fun onServerConnectionStateChanged(
            device: BluetoothDevice,
            result: TransactionResult,
            connection: GattServerConnection
    ) {
        Timber.d("Handle ServerConnectionStateChanged call from device ${device.address}, result: $result")
        when (result.resultStatus) {
            TransactionResultStatus.SUCCESS -> handleGattServerConnected(device)
            else -> handleGattServerDisconnected(device)
        }
    }

    @Synchronized
    private fun getDataSubject(device: BluetoothDevice): Subject<PeripheralConnectionStatus> {
        return registry[device] ?: add(device)
    }

    @Synchronized
    private fun add(device: BluetoothDevice): Subject<PeripheralConnectionStatus> {
        val dataSubject = BehaviorSubject.create<PeripheralConnectionStatus>()
        registry[device] = dataSubject
        return dataSubject
    }

    private fun handleGattServerConnected(device: BluetoothDevice) =
            getDataSubject(device).onNext(PeripheralConnectionStatus.CONNECTED)

    private fun handleGattServerDisconnected(device: BluetoothDevice) =
            getDataSubject(device).onNext(PeripheralConnectionStatus.DISCONNECTED)

}

package com.fitbit.bluetooth.fbgatt.rx.server.listeners

import android.bluetooth.BluetoothDevice
import com.fitbit.bluetooth.fbgatt.GattServerConnection
import com.fitbit.bluetooth.fbgatt.TransactionResult
import io.reactivex.Observable
import io.reactivex.subjects.PublishSubject
import timber.log.Timber

object GattServerConnectionChangeListenerForPeripheralMode : BaseServerConnectionEventListener {

    private val connectionStatus = PublishSubject.create<ConnectionEvent>()

    override fun onServerConnectionStateChanged(
        device: BluetoothDevice,
        result: TransactionResult,
        connection: GattServerConnection
    ) {
        Timber.d("Handle ServerConnectionStateChanged call from device ${device.address}, result: $result")
        when (result.resultStatus) {
            TransactionResult.TransactionResultStatus.SUCCESS -> handleGattServerConnected(device, connection)
            else -> handleGattServerDisconnected(device)
        }
    }

    private fun handleGattServerConnected(device: BluetoothDevice, connection: GattServerConnection) {
        connectionStatus.onNext(ConnectionEvent.Connected(device, connection))
    }

    private fun handleGattServerDisconnected(device: BluetoothDevice) =
        connectionStatus.onNext(ConnectionEvent.Disconnected(device))

    fun getDataObservable(): Observable<ConnectionEvent> = connectionStatus.hide()
}




sealed class ConnectionEvent {

    data class Connected(val centralDevice: BluetoothDevice, val connection: GattServerConnection) : ConnectionEvent()

    data class Disconnected(val centralDevice: BluetoothDevice) : ConnectionEvent()
}

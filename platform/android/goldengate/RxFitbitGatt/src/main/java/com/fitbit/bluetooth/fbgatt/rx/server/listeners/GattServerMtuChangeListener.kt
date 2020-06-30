// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.bluetooth.fbgatt.rx.server.listeners

import android.bluetooth.BluetoothDevice
import com.fitbit.bluetooth.fbgatt.FitbitGatt
import com.fitbit.bluetooth.fbgatt.GattServerConnection
import com.fitbit.bluetooth.fbgatt.TransactionResult
import com.fitbit.bluetooth.fbgatt.TransactionResult.TransactionResultStatus
import com.fitbit.bluetooth.fbgatt.rx.PeripheralConnectionStatus
import io.reactivex.Emitter
import io.reactivex.Observable
import io.reactivex.subjects.BehaviorSubject
import io.reactivex.subjects.Subject
import timber.log.Timber

/**
 * GATT server listener that listens to MTU size changes with a remote device.
 */
object GattServerMtuChangeListener : BaseServerConnectionEventListener {

    private val registry = hashMapOf<BluetoothDevice, Subject<Int>>()

    /**
     * Observable on which changes to Gatt server MTU changes are available
     *
     * @param device remote device for which connection state change need to be observer
     * @return observable to broadcast Gatt server MTU size changes per device
     */
    fun getMtuSizeObservable(device: BluetoothDevice): Observable<Int> =
        getMtuSizeSubject(device).hide()

    override fun onServerMtuChanged(device: BluetoothDevice, result: TransactionResult, connection: GattServerConnection) {
        Timber.d("Got onServerMtuChanged call from device ${device.address}, status: ${result.resultStatus}, mtu: ${result.mtu}")
        when (result.resultStatus) {
            TransactionResultStatus.SUCCESS -> handleGattServerMtuSizeChanged(device, result.mtu)
            else -> Timber.w("onMtuChanged transaction failed for $device")
        }
    }

    @Synchronized
    private fun getMtuSizeSubject(device: BluetoothDevice): Subject<Int> {
        return registry[device] ?: add(device)
    }

    @Synchronized
    private fun add(device: BluetoothDevice): Subject<Int> {
        val mtuSizeSubject = BehaviorSubject.create<Int>()
        registry[device] = mtuSizeSubject
        return mtuSizeSubject
    }

    private fun handleGattServerMtuSizeChanged(device: BluetoothDevice, mtu: Int) =
        getMtuSizeSubject(device).onNext(mtu)
}

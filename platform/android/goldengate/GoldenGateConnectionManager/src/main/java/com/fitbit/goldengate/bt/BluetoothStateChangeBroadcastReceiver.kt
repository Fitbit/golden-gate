// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bt

import android.bluetooth.BluetoothAdapter
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import io.reactivex.Observable
import io.reactivex.subjects.BehaviorSubject
import timber.log.Timber
import java.util.concurrent.atomic.AtomicBoolean

/**
 * Bluetooth Adapter state change BroadcastReceiver
 */
class BluetoothStateChangeBroadcastReceiver : BroadcastReceiver() {

    private val isRegistered = AtomicBoolean(false)
    private val bluetoothStateChangeSubject = BehaviorSubject.create<BluetoothState>()

    /**
     * Register BroadcastReceiver for Bluetooth Adapter state changes
     *
     * @return Observable on which Bluetooth Adapter state change will be available
     */
    @Synchronized
    fun register(context: Context): Observable<BluetoothState> {
        if (!isRegistered.getAndSet(true)) {
            context.registerReceiver(this, IntentFilter(BluetoothAdapter.ACTION_STATE_CHANGED))
        }
        return bluetoothStateChangeSubject
    }

    /**
     * Unregister this listener.
     *
     * After this is called no
     */
    @Synchronized
    fun unregister(context: Context) {
        context.unregisterReceiver(this)
        isRegistered.set(false)
    }

    override fun onReceive(context: Context, intent: Intent) {
        when (intent.action) {
            BluetoothAdapter.ACTION_STATE_CHANGED -> {
                val state = intent.getIntExtra(BluetoothAdapter.EXTRA_STATE, BluetoothAdapter.ERROR)
                onBluetoothStateChange(state)
            }
        }
    }

    private fun onBluetoothStateChange(state: Int) {
        val bluetoothState = BluetoothState.getValue(state)
        Timber.d("BluetoothStateChange changed to $bluetoothState")
        bluetoothStateChangeSubject.onNext(bluetoothState)
    }
}

/**
 * Indicates the local Bluetooth adapter state
 */
enum class BluetoothState(private val value: Int) {
    /** Indicates the local Bluetooth adapter is on */
    ON(BluetoothAdapter.STATE_ON),
    /** Indicates the local Bluetooth adapter is off */
    OFF(BluetoothAdapter.STATE_OFF),
    /** Indicates the local Bluetooth adapter is turning on */
    TURNING_ON(BluetoothAdapter.STATE_TURNING_ON),
    /** Indicates the local Bluetooth adapter is turning off */
    TURNING_OFF(BluetoothAdapter.STATE_TURNING_OFF),
    /** */
    UNKNOWN(-1);

    companion object {
        /**
         * Convert int value to [BluetoothState]
         */
        fun getValue(value: Int): BluetoothState {
            for (state in BluetoothState.values()) {
                if (state.value == value) return state
            }
            return UNKNOWN
        }
    }
}

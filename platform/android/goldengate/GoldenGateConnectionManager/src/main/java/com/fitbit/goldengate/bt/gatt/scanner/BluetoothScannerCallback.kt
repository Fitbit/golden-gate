// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bt.gatt.scanner

import com.fitbit.bluetooth.fbgatt.GattConnection
import com.fitbit.bluetooth.fbgatt.rx.BaseFitbitGattCallback
import io.reactivex.ObservableEmitter
import timber.log.Timber

/**
 * Callback that will listen to new devices and notify the emitter when it finds one.
 * @param emitter The emitter on which to notify a new device was found.
 */
class BluetoothScannerCallback(val e: ObservableEmitter<GattConnection>) : BaseFitbitGattCallback() {

    override fun onBluetoothPeripheralDiscovered(connection: GattConnection) {
        Timber.d("Found device $connection")
        e.onNext(connection)
    }
}

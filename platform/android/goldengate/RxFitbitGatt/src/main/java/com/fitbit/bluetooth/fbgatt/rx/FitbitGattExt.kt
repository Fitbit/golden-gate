// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.bluetooth.fbgatt.rx

import com.fitbit.bluetooth.fbgatt.FitbitGatt
import com.fitbit.bluetooth.fbgatt.GattConnection
import io.reactivex.Maybe

/**
 * Reactive extension to get [GattConnection] for given [bluetoothAddress].
 *
 * If BT is off this method will return NULL, else it will return a known connection. If there is no
 * known connection bitgatt will create a new [GattConnection] instance for this peripheral
 *
 * This method *could* return [GattConnection] even if it has been previously discovered or not,
 * so no assumption should be made about connection state. to check connection state refer [GattConnection.state]
 */
fun FitbitGatt.getGattConnection(bluetoothAddress: String): Maybe<GattConnection> =
        Maybe.fromCallable { getConnectionForBluetoothAddress(bluetoothAddress) }

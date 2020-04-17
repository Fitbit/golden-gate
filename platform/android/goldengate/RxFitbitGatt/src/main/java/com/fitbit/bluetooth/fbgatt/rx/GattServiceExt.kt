// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.bluetooth.fbgatt.rx

import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattService
import com.fitbit.bluetooth.fbgatt.GattServerConnection
import io.reactivex.Single
import java.util.UUID

/**
 * Emit a [BluetoothGattService] if the service is found
 *
 * @throws GattServiceNotFoundException if service is NOT found
 * @throws GattServerNotFoundException if GATT server is NOT available
 */
fun GattServerConnection.getGattService(serviceUUID: UUID): Single<BluetoothGattService> {
    return Single.create { emitter ->
        server?.let {
            it.getService(serviceUUID)?.let { ggService ->
                emitter.onSuccess(ggService)
            } ?: emitter.onError(GattServiceNotFoundException(serviceUUID))
        } ?: emitter.onError(GattServerNotFoundException())
    }
}

/**
 * Emit a [BluetoothGattCharacteristic] if the characteristic is found
 *
 * @throws GattCharacteristicException if characteristic is NOT found
 */
fun BluetoothGattService.getGattCharacteristic(characteristicUUID: UUID): Single<BluetoothGattCharacteristic> {
    return Single.create { emitter ->
        getCharacteristic(characteristicUUID)?.let { txGattCharacteristic ->
            emitter.onSuccess(txGattCharacteristic)
        } ?: emitter.onError(GattCharacteristicException(characteristicUUID))
    }
}

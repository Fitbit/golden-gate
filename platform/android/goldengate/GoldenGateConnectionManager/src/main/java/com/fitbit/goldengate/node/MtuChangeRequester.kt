// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.node

import com.fitbit.bluetooth.fbgatt.rx.client.BitGattPeripheral
import com.fitbit.goldengate.bindings.stack.Stack
import io.reactivex.Scheduler
import io.reactivex.Single
import io.reactivex.schedulers.Schedulers
import timber.log.Timber
import java.util.concurrent.TimeUnit

// Default minimum MTU value
internal const val DEFAULT_MIN_MTU = 23
internal const val MTU_UPDATE_TIMEOUT_SECONDS = 60L

/**
 * Class to request MTU change for a CONNECTED Node
 *
 * @param mtu MTU request value
 * @param bluetoothAddress bluetoothAddress of Node for which MTU change is requested
 * @param stack stack for connected Node
 */
class MtuChangeRequester(
    private val bluetoothAddress: String,
    private val stack: Stack,
    private val updateMtuTimeoutSeconds: Long = MTU_UPDATE_TIMEOUT_SECONDS,
    private val timeoutScheduler: Scheduler = Schedulers.computation()
) {

    /**
     * Request to set MTU for bluetooth connection and stack for connected Node
     *
     * @param peripheral connected peripheral
     * @param mtu MTU request value
     * @return A [Single] emitting an integer containing the MTU size
     */
    fun requestMtu(peripheral: BitGattPeripheral, mtu: Int): Single<Int> {
        return updatePeripheralMtu(peripheral, mtu)
            .onErrorReturn {
                // use minimum MTU value if there is any error in updating BT MTU for connected device
                Timber.w(it, "Error updating peripheral MTU value for $bluetoothAddress to $mtu")
                DEFAULT_MIN_MTU
            }
            .flatMap { peripheralMtu -> updateStackMtu(peripheralMtu) }
            .timeout(updateMtuTimeoutSeconds, TimeUnit.SECONDS, timeoutScheduler)
    }

    private fun updatePeripheralMtu(peripheral: BitGattPeripheral, mtu: Int): Single<Int> {
        return peripheral.requestMtu(mtu)
                .doOnSubscribe { Timber.d("Updating peripheral MTU: $bluetoothAddress to $mtu") }
                .doOnSuccess { Timber.d("Successfully updated peripheral MTU: $bluetoothAddress to $mtu") }
                .doOnError { Timber.w("Failed to updated peripheral MTU: $bluetoothAddress to $mtu") }
    }

    /**
     * Request to update Stack MTU
     */
    fun updateStackMtu(mtu: Int): Single<Int> {
        return Single.fromCallable { stack.updateMtu(mtu) }
                .doOnSuccess { mtuUpdated ->
                    if(!mtuUpdated) throw  MtuChangeException("Failed to update stack MTU to $mtu for node: $bluetoothAddress")
                }
                .map { mtu }
                .doOnSubscribe { Timber.d("Updating stack MTU: $mtu") }
                .doOnSuccess { Timber.d("Successfully updated stack MTU: $mtu") }
                .doOnError { Timber.w("Failed to updated stack MTU:$mtu") }
    }
}

/**
 * Exception thrown when failed to update MTU
 */
class MtuChangeException(
        message: String? = null,
        cause: Throwable? = null
) : Exception(message, cause)

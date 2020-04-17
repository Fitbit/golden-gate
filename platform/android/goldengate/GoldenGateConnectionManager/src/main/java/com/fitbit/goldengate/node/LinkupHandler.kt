// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.node

import com.fitbit.bluetooth.fbgatt.rx.client.BitGattPeripheral
import com.fitbit.bluetooth.fbgatt.rx.client.GattServiceRefresher
import com.fitbit.bluetooth.fbgatt.rx.client.PeripheralServiceSubscriber
import com.fitbit.goldengate.bt.gatt.client.services.GattDatabaseValidator
import com.fitbit.goldengate.bt.gatt.client.services.GenericAttributeService
import com.fitbit.goldengate.bt.gatt.server.services.gattlink.GattlinkService
import com.fitbit.goldengate.bt.gatt.server.services.gattlink.TransmitCharacteristic
import com.fitbit.goldengate.bt.gatt.util.dumpServices
import io.reactivex.Completable
import io.reactivex.Flowable
import io.reactivex.Scheduler
import io.reactivex.schedulers.Schedulers
import timber.log.Timber
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicInteger

// Default number of times to attempt
internal const val MAX_RETRY_ATTEMPTS = 3
internal const val LINK_UP_TIMEOUT_SECONDS = 60L

/**
 * Utility class to establish a link with given CONNECTED Node
 */
internal class LinkupHandler(
    private val peripheralServiceSubscriber: PeripheralServiceSubscriber = PeripheralServiceSubscriber(),
    private val gattDatabaseValidator: GattDatabaseValidator = GattDatabaseValidator(),
    private val gattServiceRefresher: GattServiceRefresher = GattServiceRefresher(),
    private val maxRetryAttempts: Int = MAX_RETRY_ATTEMPTS,
    private val linkUpTimeoutSeconds: Long = LINK_UP_TIMEOUT_SECONDS,
    private val timeoutScheduler: Scheduler = Schedulers.computation()
) {

    /**
     * Create a link with the Node. A link is established when Hub(Mobile) subscribes to
     * [GoldenGateService] and Node(Tracker) subscribes to [GattlinkService].
     *
     * It is required that Node is already connected
     *
     * @param peripheral a connected Node
     * @return Emits [Completable] if connected, error otherwise
     */
    // TODO: FC-2433 - subscribing all the time is not optimal and we want to only subscribe when we know its not already subscribed
    fun link(peripheral: BitGattPeripheral): Completable {
        return discoverServicesAndValidateGattDatabase(peripheral)
            // We first try to subscribe to Gattlink service on Node
            .andThen(subscribeToGattlink(peripheral))
            .doOnComplete { Timber.d("Successfully subscribed to Gattlink service on Node: ${peripheral.fitbitDevice.address}") }
            .doOnError { t -> Timber.e(t, "Failed to subscribed to Gattlink service on Node: ${peripheral.fitbitDevice.address}") }
            .timeout(linkUpTimeoutSeconds, TimeUnit.SECONDS, timeoutScheduler)
    }

    private fun discoverServicesAndValidateGattDatabase(peripheral: BitGattPeripheral): Completable {
        return discoverServices(peripheral)
            .andThen(subscribeToGenericAttribute(peripheral))
            .andThen(validateGattDatabase(peripheral))
            // TODO: FC-2979 - consider to pull rx utils into a common repo and to replace retryWhen with retryUtil
            .retryWhen { errors ->
                val counter = AtomicInteger()
                errors.takeWhile { counter.getAndIncrement() <= maxRetryAttempts }
                    .flatMap { e ->
                        val retryCount = counter.get()
                        if (retryCount > maxRetryAttempts) {
                            throw e
                        }
                        Timber.d("delay retry by ${retryCount * 2}  seconds")
                        Flowable.timer(retryCount * 2000L, TimeUnit.MILLISECONDS, Schedulers.computation())
                    }
            }
            .onErrorResumeNext {
                Timber.e("Failed to validate the gatt service")
                refreshServices(peripheral)
                    .andThen(discoverServices(peripheral)) }
    }

    private fun discoverServices(peripheral: BitGattPeripheral): Completable = Completable.defer {
        peripheral.discoverServices()
            .doOnSuccess { dumpServices(peripheral.services) }
            .doOnError { Timber.w(it, "Error discovering services on $peripheral") }
            .ignoreElement()
    }

    private fun subscribeToGenericAttribute(peripheral: BitGattPeripheral): Completable {
        return Completable.defer {
            peripheralServiceSubscriber.subscribe(
                peripheral,
                GenericAttributeService.uuid,
                GenericAttributeService.ServiceChangedCharacteristic.uuid,
                isIndication = true
            )
        }.doOnError { Timber.w(it, "failed to subscribe service changed characteristic") }
            // some trackers might not support this service
            // ignore the error for now
            .onErrorComplete()
    }

    private fun validateGattDatabase(peripheral: BitGattPeripheral): Completable =
        Completable.defer {
            gattDatabaseValidator.validate(peripheral)
        }

    private fun subscribeToGattlink(peripheral: BitGattPeripheral): Completable {
        return Completable.defer {
            peripheralServiceSubscriber.subscribe(
                peripheral,
                GattlinkService.uuid,
                TransmitCharacteristic.uuid
            )
        }
    }

    private fun refreshServices(peripheral: BitGattPeripheral): Completable =
        gattServiceRefresher
            .refresh(peripheral.gattConnection)
            .doOnError { Timber.w(it, "Fail to refresh services on $peripheral") }
            .doOnComplete { Timber.d("Successfully refresh services on $peripheral") }
}

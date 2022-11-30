// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.node

import com.fitbit.bluetooth.fbgatt.rx.client.BitGattPeer
import com.fitbit.bluetooth.fbgatt.rx.client.GattServiceDiscoverer
import com.fitbit.bluetooth.fbgatt.rx.client.GattServiceRefresher
import com.fitbit.bluetooth.fbgatt.rx.client.PeerGattServiceSubscriber
import com.fitbit.goldengate.bt.gatt.client.services.GattDatabaseValidator
import com.fitbit.goldengate.bt.gatt.client.services.GenericAttributeService
import com.fitbit.goldengate.bt.gatt.server.services.gattlink.FitbitGattlinkService
import com.fitbit.goldengate.bt.gatt.server.services.gattlink.GattlinkService
import com.fitbit.goldengate.bt.gatt.server.services.gattlink.TransmitCharacteristic
import io.reactivex.Completable
import io.reactivex.Flowable
import io.reactivex.Scheduler
import io.reactivex.schedulers.Schedulers
import timber.log.Timber
import java.util.UUID
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicInteger

// Default number of times to attempt
internal const val MAX_RETRY_ATTEMPTS = 3
internal const val LINK_UP_TIMEOUT_SECONDS = 60L

// add delay after GATT refresh and before discovery as refresh is a async call, and we want to give it little time to complete b4 calling discovery again
internal const val GATT_SERVICE_REFRESH_DELAY_IN_SEC = 2L

/**
 * Utility class to establish a link with given CONNECTED Node
 */
internal class LinkupWithPeerNodeHandler(
    private val peerGattServiceSubscriber: PeerGattServiceSubscriber = PeerGattServiceSubscriber(),
    private val gattDatabaseValidator: GattDatabaseValidator = GattDatabaseValidator(),
    private val gattServiceRefresher: GattServiceRefresher = GattServiceRefresher(),
    private val gattServiceDiscoverer: GattServiceDiscoverer = GattServiceDiscoverer(),
    private val maxRetryAttempts: Int = MAX_RETRY_ATTEMPTS,
    private val linkUpTimeoutSeconds: Long = LINK_UP_TIMEOUT_SECONDS,
    private val timeoutScheduler: Scheduler = Schedulers.computation()
): Linkup {

    /**
     * Create a link with the Node. A link is established when Hub(Mobile) subscribes to
     * [GoldenGateService] and Node(Tracker) subscribes to [GattlinkService].
     *
     * It is required that Node is already connected
     *
     * @param peer a connected Node
     * @return Emits [Completable] if connected, error otherwise
     */
    // TODO: FC-2433 - subscribing all the time is not optimal and we want to only subscribe when we know its not already subscribed
    override fun link(peer: BitGattPeer): Completable {
        return discoverServicesAndValidateGattDatabase(peer)
            // We first try to subscribe to Gattlink service on Node
            .andThen(subscribeToGattlink(peer))
            .doOnComplete { Timber.d("Successfully subscribed to Gattlink service on Node: ${peer.fitbitDevice.address}") }
            .doOnError { t -> Timber.e(t, "Failed to subscribed to Gattlink service on Node: ${peer.fitbitDevice.address}") }
            .timeout(linkUpTimeoutSeconds, TimeUnit.SECONDS, timeoutScheduler)
    }

    private fun discoverServicesAndValidateGattDatabase(peer: BitGattPeer): Completable {
        return discoverServices(peer)
            .andThen(subscribeToGenericAttribute(peer))
            .andThen(validateGattDatabase(peer))
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
            .onErrorResumeNext { error ->
                Timber.e(error, "Failed to validate the gatt service")
                refreshServices(peer)
                    .andThen(Completable.timer(GATT_SERVICE_REFRESH_DELAY_IN_SEC, TimeUnit.SECONDS))
                    .andThen(discoverServices(peer))
            }
    }

    private fun discoverServices(peer: BitGattPeer): Completable = Completable.defer {
        gattServiceDiscoverer.discover(peer.gattConnection).ignoreElement()
    }

    private fun subscribeToGenericAttribute(peer: BitGattPeer): Completable {
        return Completable.defer {
            peerGattServiceSubscriber.subscribe(
                peer,
                GenericAttributeService.uuid,
                GenericAttributeService.ServiceChangedCharacteristic.uuid,
                isIndication = true
            )
        }.doOnError { Timber.w(it, "failed to subscribe service changed characteristic") }
            // some trackers might not support this service
            // ignore the error for now
            .onErrorComplete()
    }

    private fun validateGattDatabase(peer: BitGattPeer): Completable =
        Completable.defer {
            gattDatabaseValidator.validate(peer)
        }

    private fun subscribeToGattlink(peer: BitGattPeer): Completable {
        return Completable.defer {
            peerGattServiceSubscriber.subscribe(
                peer,
                getRemoteGattLinkServiceUUID(peer),
                TransmitCharacteristic.uuid
            )
        }
    }

    private fun getRemoteGattLinkServiceUUID(peer: BitGattPeer): UUID {
        return if (peer.gattConnection.getRemoteGattService(GattlinkService.uuid) != null)
            GattlinkService.uuid else FitbitGattlinkService.uuid
    }

    private fun refreshServices(peer: BitGattPeer): Completable =
        gattServiceRefresher.refresh(peer.gattConnection)
}

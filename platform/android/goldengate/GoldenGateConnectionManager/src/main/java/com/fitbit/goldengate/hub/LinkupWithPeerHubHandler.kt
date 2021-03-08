package com.fitbit.goldengate.hub

import com.fitbit.bluetooth.fbgatt.rx.GattCharacteristicSubscriptionStatus
import com.fitbit.bluetooth.fbgatt.rx.client.BitGattPeer
import com.fitbit.bluetooth.fbgatt.rx.client.GattServiceDiscoverer
import com.fitbit.bluetooth.fbgatt.rx.client.GattServiceRefresher
import com.fitbit.bluetooth.fbgatt.rx.client.PeerGattServiceSubscriber
import com.fitbit.goldengate.bt.gatt.server.services.gattlink.GattlinkService
import com.fitbit.goldengate.bt.gatt.server.services.gattlink.listeners.TransmitCharacteristicSubscriptionListener
import com.fitbit.goldengate.node.GATT_SERVICE_REFRESH_DELAY_IN_SEC
import com.fitbit.goldengate.node.Linkup
import com.fitbit.linkcontroller.services.configuration.ClientPreferredConnectionConfigurationCharacteristic
import com.fitbit.linkcontroller.services.configuration.ClientPreferredConnectionModeCharacteristic
import com.fitbit.linkcontroller.services.configuration.GeneralPurposeCommandCharacteristic
import com.fitbit.linkcontroller.services.configuration.LinkConfigurationService
import io.reactivex.Completable
import io.reactivex.Observable
import io.reactivex.Scheduler
import io.reactivex.schedulers.Schedulers
import timber.log.Timber
import java.util.concurrent.TimeUnit

internal const val LINK_UP_TIMEOUT_SECONDS = 60L

/**
 * Utility class to establish a link with given CONNECTED Hub
 */
internal class LinkupWithPeerHubHandler(
    private val peerGattServiceSubscriber: PeerGattServiceSubscriber = PeerGattServiceSubscriber(),
    private val transmitCharacteristicSubscriptionListener: TransmitCharacteristicSubscriptionListener = TransmitCharacteristicSubscriptionListener.instance,
    private val linkUpTimeoutSeconds: Long = LINK_UP_TIMEOUT_SECONDS,
    private val timeoutScheduler: Scheduler = Schedulers.computation(),
    private val gattServiceRefresher: GattServiceRefresher = GattServiceRefresher(),
    private val gattServiceDiscoverer: GattServiceDiscoverer = GattServiceDiscoverer()
): Linkup {

    /**
     * Create a link with the Hub. A link is established when Hub(Mobile) subscribes to
     * [GoldenGateService] and Node(Tracker) subscribes to [GattlinkService].
     *
     * It is required that Node is already connected
     *
     * @param peer a connected Node
     * @return Emits [Completable] if connected, error otherwise
     */
    override fun link(peer: BitGattPeer): Completable {
        return peer.connect().ignoreElement()
            .andThen(refreshServices(peer))
            .andThen(Completable.timer(GATT_SERVICE_REFRESH_DELAY_IN_SEC, TimeUnit.SECONDS))
            .andThen(discoverServices(peer))
            .andThen(subscribeToPreferredConnectionConfigurationCharacteristic(peer))
            .andThen(subscribeToPreferredConnectionModeCharacteristic(peer))
            .andThen(subscribeToGeneralPurposeCommandCharacteristic(peer))
            .andThen(waitForGattlinkSubscription(peer))
            .doOnComplete { Timber.d("Local Gattlink service and remote Link Configuration Service on Hub: ${peer.fitbitDevice.address} have been subscribed") }
            .doOnError { Timber.e(it, "Local Gattlink service and remote Link Configuration Service on Hub: ${peer.fitbitDevice.address} haven't been subscribed") }
            .timeout(linkUpTimeoutSeconds, TimeUnit.SECONDS, timeoutScheduler)
            .doOnError { Timber.e(it, "Failed to complete linkup handler within timeout") }
    }

    private fun discoverServices(peer: BitGattPeer): Completable = Completable.defer {
        gattServiceDiscoverer.discover(peer.gattConnection).ignoreElement()
    }

    private fun subscribeToPreferredConnectionConfigurationCharacteristic(peer: BitGattPeer): Completable {
        return Completable.defer {
            Timber.d("<1> subscribe ClientPreferredConnectionConfigurationCharacteristic")
            peerGattServiceSubscriber.subscribe(
                peer,
                LinkConfigurationService.uuid,
                ClientPreferredConnectionConfigurationCharacteristic.uuid
            )
        }
    }

    private fun subscribeToPreferredConnectionModeCharacteristic(peer: BitGattPeer): Completable {
        return Completable.defer {
            Timber.d("<2> subscribe ClientPreferredConnectionModeCharacteristic")
            peerGattServiceSubscriber.subscribe(
                peer,
                LinkConfigurationService.uuid,
                ClientPreferredConnectionModeCharacteristic.uuid
            )
        }
    }

    private fun subscribeToGeneralPurposeCommandCharacteristic(peer: BitGattPeer): Completable {
        return Completable.defer {
            Timber.d("<3> subscribe GeneralPurposeCommandCharacteristic")
            peerGattServiceSubscriber.subscribe(
                peer,
                LinkConfigurationService.uuid,
                GeneralPurposeCommandCharacteristic.uuid
            )
        }
    }

    private fun waitForGattlinkSubscription(peer: BitGattPeer): Completable =
        Observable.defer {
            Timber.d("<4> wait for Gattlink subscription")
            transmitCharacteristicSubscriptionListener.getDataObservable(peer.fitbitDevice.btDevice)
        }.filter { status -> status == GattCharacteristicSubscriptionStatus.ENABLED }
            .firstOrError()
            .ignoreElement()

    private fun refreshServices(peer: BitGattPeer): Completable =
        gattServiceRefresher
            .refresh(peer.gattConnection)
            .onErrorComplete()
}

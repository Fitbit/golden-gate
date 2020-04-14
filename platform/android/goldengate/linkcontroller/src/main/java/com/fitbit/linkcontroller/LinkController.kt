package com.fitbit.linkcontroller

import com.fitbit.bluetooth.fbgatt.GattConnection
import com.fitbit.bluetooth.fbgatt.rx.client.BitGattPeripheral
import com.fitbit.bluetooth.fbgatt.rx.client.GattCharacteristicReader
import com.fitbit.bluetooth.fbgatt.rx.client.PeripheralServiceSubscriber
import com.fitbit.bluetooth.fbgatt.rx.client.listeners.GattClientCharacteristicChangeListener
import com.fitbit.bluetooth.fbgatt.rx.server.GattCharacteristicNotifier
import com.fitbit.linkcontroller.services.configuration.ClientPreferredConnectionConfigurationCharacteristic
import com.fitbit.linkcontroller.services.configuration.ClientPreferredConnectionModeCharacteristic
import com.fitbit.linkcontroller.services.configuration.GattCharacteristicSubscriptionStatus
import com.fitbit.linkcontroller.services.configuration.GeneralPurposeCommandCharacteristic
import com.fitbit.linkcontroller.services.configuration.GeneralPurposeCommandCode
import com.fitbit.linkcontroller.services.configuration.LinkConfigurationService
import com.fitbit.linkcontroller.services.configuration.PreferredConnectionConfiguration
import com.fitbit.linkcontroller.services.configuration.PreferredConnectionMode
import com.fitbit.linkcontroller.services.status.CurrentConnectionConfiguration
import com.fitbit.linkcontroller.services.status.CurrentConnectionStatus
import com.fitbit.linkcontroller.services.status.LinkStatusService
import io.reactivex.Completable
import io.reactivex.Observable
import io.reactivex.Single
import timber.log.Timber

private val DEFAULT_PREFERRED_CONNECTION_MODE = PreferredConnectionMode.SLOW

/**
 * This class handles Connection parameters for a specific bluetooth peripheral device
 * It provides an api with setting preferred connection config and connection mode as well as
 * getting the current connection configuration and status between the mobile app and peripheral
 */
class LinkController internal constructor(
    private val gattConnection: GattConnection,
    private val linkConfigurationSubscriptionObservable: Observable<GattCharacteristicSubscriptionStatus>,
    private val linkConfigurationCharacteristicNotifier: GattCharacteristicNotifier = GattCharacteristicNotifier(
        gattConnection.device.btDevice
    ),
    private val rxBlePeripheral: BitGattPeripheral = BitGattPeripheral(gattConnection),
    private val gattCharacteristicReader: GattCharacteristicReader =  GattCharacteristicReader(gattConnection),
    private val gattClientCharacteristicChangeListener: GattClientCharacteristicChangeListener = GattClientCharacteristicChangeListener(),
    private val peripheralServiceSubscriber: PeripheralServiceSubscriber = PeripheralServiceSubscriber()
) {
    private var preferredConnectionConfiguration = PreferredConnectionConfiguration()
    private var preferredConnectionMode = DEFAULT_PREFERRED_CONNECTION_MODE
    private var generalPurposeCommand = GeneralPurposeCommandCode.RESERVED


    // Link Status Service - hosted on the Tracker

    /**
     * Reads the current connection configuration from the Peripheral device.
     * It will read the [LinkStatusService.currentConfigurationUuid] characteristic and parse its result
     * It will also notify all observers to this change
     * @return a single with the [CurrentConnectionConfiguration]
     */
    fun getCurrentConnectionConfiguration(): Single<CurrentConnectionConfiguration> {
        return gattCharacteristicReader.read(LinkStatusService.uuid, LinkStatusService.currentConfigurationUuid)
            .map { data -> CurrentConnectionConfiguration.parseFromByteArray(data) }
    }

    /**
     * Subscribes to receive notifications for [CurrentConnectionConfiguration].
     * An observer needs to be subscribed to the [observeCurrentConnectionConfiguration]
     * @see observeCurrentConnectionConfiguration
     */
    fun subscribeToCurrentConnectionConfigurationNotifications(): Completable {
        return peripheralServiceSubscriber.subscribe(
            rxBlePeripheral,
            LinkStatusService.uuid,
            LinkStatusService.currentConfigurationUuid
        )
            .doOnSubscribe { Timber.i("Subscribing to CurrentConnectionConfiguration for $gattConnection") }
            .doOnComplete { Timber.i("Subscribed to CurrentConnectionConfiguration for $gattConnection") }
            .doOnError { t -> Timber.e(t, "Error subscribing to CurrentConnectionConfiguration for $gattConnection") }
    }

    /**
     * Returns an observable with all the changes for the [CurrentConnectionConfiguration]
     * Changes occurs when the tracker notifies the mobile app or when the app tries to read the tracker characteristic
     * @see subscribeToCurrentConnectionConfigurationNotifications
     * @see getCurrentConnectionConfiguration
     */
    fun observeCurrentConnectionConfiguration(): Observable<CurrentConnectionConfiguration> {
        return gattClientCharacteristicChangeListener.register(
            gattConnection,
            LinkStatusService.currentConfigurationUuid
        ).map { data -> CurrentConnectionConfiguration.parseFromByteArray(data) }
    }

    /**
     * Reads the current connection status from the Peripheral device.
     * It will read the [LinkStatusService.currentConnectionModeUuid] characteristic and parse its result
     * It will also notify all observers to this change
     * @return a single with the [CurrentConnectionStatus]
     */
    fun getCurrentConnectionStatus(): Single<CurrentConnectionStatus> {
        return gattCharacteristicReader.read(LinkStatusService.uuid, LinkStatusService.currentConnectionModeUuid)
            .map { data -> CurrentConnectionStatus.parseFromByteArray(data) }
    }

    /**
     * Subscribes to receive notifications for [CurrentConnectionStatus].
     * An observer needs to be subscribed to the [observeCurrentConnectionStatus]
     * @see observeCurrentConnectionStatus
     */
    fun subscribeToCurrentConnectionStatusNotifications(): Completable {
        return peripheralServiceSubscriber.subscribe(
            rxBlePeripheral,
            LinkStatusService.uuid,
            LinkStatusService.currentConnectionModeUuid
        )
            .doOnSubscribe { Timber.i("Subscribing to CurrentConnectionStatus for $gattConnection") }
            .doOnComplete { Timber.i("Subscribed to CurrentConnectionStatus for $gattConnection") }
            .doOnError { t -> Timber.e(t, "Error subscribing to CurrentConnectionStatus for $gattConnection") }
    }

    /**
     * Returns an observable with all the changes for the [CurrentConnectionStatus]
     * Changes occurs when the tracker notifies the mobile app or when the app tries to read the tracker characteristic
     * @see subscribeToCurrentConnectionStatusNotifications
     * @see getCurrentConnectionStatus
     */
    fun observeCurrentConnectionStatus(): Observable<CurrentConnectionStatus> {
        return gattClientCharacteristicChangeListener.register(
            gattConnection,
            LinkStatusService.currentConnectionModeUuid
        ).map { data -> CurrentConnectionStatus.parseFromByteArray(data) }
    }

    // Link Configuration Service - hosted on the mobile app

    /**
     * Allows the client to set different connection parameters.
     * It sets the characteristic with the correct value, and notifies all listeners
     * @param preferredConnectionConfiguration The client can set connection interval, slave latency,
     * supervision timeout, mtu, dle using this Object
     * @throws IllegalStateException if device is not subscribed to LinkConfigurationService
     */
    fun setPreferredConnectionConfiguration(preferredConnectionConfiguration: PreferredConnectionConfiguration): Completable {
        //notify server characteristics
        this.preferredConnectionConfiguration = preferredConnectionConfiguration
        return linkConfigurationSubscriptionObservable.take(1).flatMapCompletable {
            if (it == GattCharacteristicSubscriptionStatus.ENABLED) {
                linkConfigurationCharacteristicNotifier.notify(
                    LinkConfigurationService.uuid,
                    ClientPreferredConnectionConfigurationCharacteristic.uuid,
                    preferredConnectionConfiguration.toByteArray()
                )
            } else {
                Completable.error(IllegalStateException("Device not subscribed to the Link Configuration Service"))
            }
        }

    }

    /**
     * Allows the client to set different connection mode.
     * It sets the characteristic with the correct value, and notifies all listeners
     * @param connectionMode The client can set connection mode either fast or slow,
     * @throws IllegalStateException if device is not subscribed to LinkConfigurationService
     */
    fun setPreferredConnectionMode(connectionMode: PreferredConnectionMode): Completable {
        Timber.d("Setting preferred connection mode to $connectionMode")
        this.preferredConnectionMode = connectionMode
        return linkConfigurationSubscriptionObservable.take(1).flatMapCompletable {
            if (it == GattCharacteristicSubscriptionStatus.ENABLED) {
                linkConfigurationCharacteristicNotifier.notify(
                    LinkConfigurationService.uuid,
                    ClientPreferredConnectionModeCharacteristic.uuid,
                    connectionMode.toByteArray()
                )
            } else {
                Completable.error(IllegalStateException("Device not subscribed to the Link Configuration Service"))
            }
        }
    }

    /**
     * Used by the [LinkConfigurationServiceEventListener] when the tracker wants to read the
     * preferred connection mode characteristic
     */
    fun getPreferredConnectionMode(): PreferredConnectionMode {
        return preferredConnectionMode
    }

    /**
     * Used by the [LinkConfigurationServiceEventListener] when the tracker wants to read the
     * preferred connection configuration characteristic
     */
    fun getPreferredConnectionConfiguration(): PreferredConnectionConfiguration {
        return preferredConnectionConfiguration
    }

    /**
     * Used by the [StackListener] when the stack is in stall state
     */
    fun setGeneralPurposeCommand(commandCode: GeneralPurposeCommandCode): Completable {
        Timber.w("Set General Purpose Command: $commandCode")
        this.generalPurposeCommand = commandCode
        return linkConfigurationSubscriptionObservable.take(1).flatMapCompletable {
            if (this.generalPurposeCommand == GeneralPurposeCommandCode.DISCONNECT) {
                linkConfigurationCharacteristicNotifier.notify(
                    LinkConfigurationService.uuid,
                    GeneralPurposeCommandCharacteristic.uuid,
                    commandCode.toByteArray()
                )
            } else {
                Completable.error(IllegalStateException("Device not subscribed to the Link Configuration Service"))
            }
        }
    }

}

// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.linkcontroller

import android.bluetooth.BluetoothDevice
import com.fitbit.bluetooth.fbgatt.FitbitBluetoothDevice
import com.fitbit.bluetooth.fbgatt.FitbitGatt
import com.fitbit.bluetooth.fbgatt.GattConnection
import com.fitbit.bluetooth.fbgatt.rx.server.BitGattServer
import com.fitbit.linkcontroller.services.configuration.ClientPreferredConnectionConfigurationCharacteristic
import com.fitbit.linkcontroller.services.configuration.ClientPreferredConnectionModeCharacteristic
import com.fitbit.linkcontroller.services.configuration.GeneralPurposeCommandCharacteristic
import com.fitbit.linkcontroller.services.configuration.LinkConfigurationService
import com.fitbit.linkcontroller.services.configuration.LinkConfigurationServiceEventListener
import io.reactivex.Completable

/**
 * This class initialises the Link Controller Configuration Service.
 * It provides to the consumers [LinkController] objects based on [FitbitBluetoothDevice]
 * These are used to configure connection parameters between the mobile app and the peripheral device
 */

class LinkControllerProvider private constructor(
    private val fitbitGatt: FitbitGatt = FitbitGatt.getInstance(),
    private val gattServer: BitGattServer = BitGattServer(),
    private val linkConfigurationService: LinkConfigurationService = LinkConfigurationService(),
    private val linkConfigurationServiceEventListener: LinkConfigurationServiceEventListener = LinkConfigurationServiceEventListener()
) {
    private val linkControllersMap = hashMapOf<BluetoothDevice, LinkController>()

    private object Holder {
        val INSTANCE = LinkControllerProvider()
    }


    companion object {
        val INSTANCE: LinkControllerProvider by lazy { Holder.INSTANCE }
    }

    init {
        linkConfigurationServiceEventListener.linkControllerProvider = this
    }

    /**
     * Get the Link controller for a specific peripheral device
     */
    @Synchronized
    fun getLinkController(device: BluetoothDevice): LinkController {
        return linkControllersMap[device] ?: add(device)
    }

    @Synchronized
    fun getLinkController(gattConnection: GattConnection): LinkController {
        return getLinkController(gattConnection.device.btDevice)
    }

    @Synchronized
    private fun add(bluetoothDevice: BluetoothDevice): LinkController {
        val linkController = LinkController(
            bluetoothDevice,
            linkConfigurationServiceEventListener.getDataObservable(
                bluetoothDevice,
                ClientPreferredConnectionModeCharacteristic.uuid
            ),
            linkConfigurationServiceEventListener.getDataObservable(
                bluetoothDevice,
                ClientPreferredConnectionConfigurationCharacteristic.uuid
            ),
            linkConfigurationServiceEventListener.getDataObservable(
                bluetoothDevice,
                GeneralPurposeCommandCharacteristic.uuid
            )
        )
        linkControllersMap[bluetoothDevice] = linkController
        return linkController
    }

    /**
     * Registers the new service and its listeners
     * This should be called by the connection manager only when bitgatt is started
     */
    fun addLinkConfigurationService(): Completable {
        return registerListeners().andThen(gattServer.addServices(linkConfigurationService))
    }

    /**
     * This registers Connection Event listener for the Link Configuration service to bitgatt.
     * These events
     */
    private fun registerListeners(): Completable {
        return Completable.fromCallable {
            fitbitGatt.server.registerConnectionEventListener(linkConfigurationServiceEventListener)
        }
    }


}

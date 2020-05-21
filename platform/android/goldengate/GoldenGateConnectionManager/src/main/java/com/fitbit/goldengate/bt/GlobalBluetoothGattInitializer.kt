// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bt

import android.content.Context
import com.fitbit.bluetooth.fbgatt.FitbitGatt
import com.fitbit.bluetooth.fbgatt.GattServerConnection
import com.fitbit.bluetooth.fbgatt.exception.BitGattStartException
import com.fitbit.bluetooth.fbgatt.rx.BaseFitbitGattCallback
import com.fitbit.bluetooth.fbgatt.rx.server.BitGattServer
import com.fitbit.goldengate.bt.gatt.GattServerListenerRegistrar
import com.fitbit.goldengate.bt.gatt.server.services.gattlink.GattlinkService
import com.fitbit.linkcontroller.LinkControllerProvider
import io.reactivex.disposables.CompositeDisposable
import io.reactivex.internal.functions.Functions.EMPTY_ACTION
import io.reactivex.schedulers.Schedulers
import timber.log.Timber

/**
 * This is a helper class that is responsible for correctly initializing [FitbitGatt],
 * registering any GATT listeners when and if necessary.
 *
 * These class also takes care of listening to bluetooth adapter state changes and performing the
 * above operation again if necessary.
 *
 * It is expected that this class is initialized during app startup and lives during app lifecycle
 */
internal class GlobalBluetoothGattInitializer(
    private val fitbitGatt: FitbitGatt = FitbitGatt.getInstance(),
    private val gattServer: BitGattServer = BitGattServer(),
    private val gattServerListenerRegistrar: GattServerListenerRegistrar = GattServerListenerRegistrar,
    private val linkControllerProvider: LinkControllerProvider = LinkControllerProvider.INSTANCE
) {

    private val disposeBag = CompositeDisposable()
    private var isBleCentral = true

    /**
     * Performs necessary initialization related to bluetooth GATT setup and listens to bluetooth adapter state changes
     */
    fun start(context: Context, isBleCentralRole: Boolean = true) {
        isBleCentral = isBleCentralRole
        fitbitGatt.registerGattEventListener(bluetoothStateListener)
        fitbitGatt.start(context)
    }

    /**
     * Not expected to be called but call this method if for any reason we want to stop listening to
     * bluetooth adapter state changes in relation ship with properly initializing GATT setup
     */
    fun stop() {
        disposeBag.clear()
    }

    private val bluetoothStateListener = object: BaseFitbitGattCallback() {
        override fun onGattServerStarted(serverConnection: GattServerConnection?) {
            if (isBleCentral) {
                Timber.d("add GATT services of central mode")
                addGattServicesForCentral()
            } else {
                Timber.d("add GATT services of peripheral mode")
                addGattServicesForPeripheral()
            }
        }

        override fun onGattClientStartError(exception: BitGattStartException?) {
            Timber.w(exception, "Bitgatt Client failed to start")
        }

        override fun onGattServerStartError(exception: BitGattStartException?) {
            Timber.w(exception, "Bitgatt Server failed to start")
        }

        override fun onScannerInitError(exception: BitGattStartException?) {
            Timber.w(exception, "Bitgatt scanner failed to initialize")
        }
    }

    private fun addGattServicesForCentral() {
        disposeBag.add(gattServerListenerRegistrar.registerGattServerListeners(fitbitGatt.server)
                .andThen(linkControllerProvider.addLinkConfigurationService())
                .subscribeOn(Schedulers.io())
                .subscribe(
                    { EMPTY_ACTION },
                    { Timber.e(it, "Error handling bluetooth state change") }
                )
        )
    }

    private fun addGattServicesForPeripheral() {
        disposeBag.add(gattServerListenerRegistrar.registerGattServerNodeListeners(fitbitGatt.server)
            .andThen(gattServer.addServices(GattlinkService()))
            .subscribeOn(Schedulers.io())
            .subscribe(
                { EMPTY_ACTION },
                { Timber.e(it, "Error handling bluetooth state change") }
            )
        )
    }
}

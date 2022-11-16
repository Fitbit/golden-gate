package com.fitbit.goldengate.bt.gatt.server.services.gattcache

import com.fitbit.bluetooth.fbgatt.FitbitGatt
import com.fitbit.bluetooth.fbgatt.rx.GattServerNotFoundException
import com.fitbit.bluetooth.fbgatt.rx.server.BitGattServer
import io.reactivex.Completable

class GattCacheServiceHandler(
    private val fitbitGatt: FitbitGatt = FitbitGatt.getInstance(),
    private val gattServer: BitGattServer = BitGattServer(),
    private val gattCacheService: GattCacheService = GattCacheService(),
    private val gattCacheServiceEventListener: GattCacheServiceEventListener = GattCacheServiceEventListener()
) {

    /**
     * Registers the new service and its listeners
     * This should be called by the connection manager only when bitgatt is started
     */
    fun addGattCacheService(): Completable {
        return registerListeners().andThen(gattServer.addServices(gattCacheService))
    }

    /**
     * This registers Connection Event listener for the Link Configuration service to bitgatt.
     * These events
     */
    private fun registerListeners(): Completable {
        return Completable.fromCallable {
            fitbitGatt.server?.registerConnectionEventListener(gattCacheServiceEventListener)
                ?: throw GattServerNotFoundException()
        }
    }
}

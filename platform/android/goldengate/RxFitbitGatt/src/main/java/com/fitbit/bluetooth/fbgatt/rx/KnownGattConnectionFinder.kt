package com.fitbit.bluetooth.fbgatt.rx

import com.fitbit.bluetooth.fbgatt.FitbitGatt
import com.fitbit.bluetooth.fbgatt.GattConnection
import io.reactivex.Single
import java.util.UUID

/**
 * Finds known [GattConnection]
 */
class KnownGattConnectionFinder(private val fitbitGatt: FitbitGatt = FitbitGatt.getInstance()) {

    /**
     * Finds known GattConnections matching given device names and [filterPredicate] filter
     *
     * @param names list of device names to match, if empty known connections will not be filtered by device names
     * @param filterPredicate filter to apply to known GattConnection, if no predicate provided will return all known connections
     * @return Returns a [Single] that will emit all known GattConnection
     */
    fun find(names: List<String> = emptyList(), filterPredicate: (GattConnection) -> Boolean = { true }): Single<List<GattConnection>> {
        return Single.just(fitbitGatt.getMatchingConnectionsForDeviceNames(names).filter(filterPredicate))
    }

    /**
     * Finds known GattConnections matching given remote services and [filterPredicate] filter
     *
     * @param services list of remote GATT service UUID's to match
     * @param filterPredicate filter to apply to known GattConnection
     * @return Returns a [Single] that will emit all known GattConnection
     */
    fun findByService(services: List<UUID>, filterPredicate: (GattConnection) -> Boolean = { true }): Single<List<GattConnection>> {
        return Single.just(fitbitGatt.getMatchingConnectionsForServices(services).filter(filterPredicate))
    }
}

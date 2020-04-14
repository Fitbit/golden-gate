package com.fitbit.bluetooth.fbgatt.rx.server

import android.bluetooth.BluetoothGattService
import com.fitbit.bluetooth.fbgatt.GattServerConnection
import com.fitbit.bluetooth.fbgatt.GattState
import com.fitbit.bluetooth.fbgatt.tx.AddGattServerServiceTransaction

/**
 * Provides bitgatt transactions for gatt-server related operations. This interface is so alternative
 * implementations, like mocking for testing.
 */
class ServerTransactionProvider {

    /**
     * Provides a bitgatt add services transaction, which will add services (and the containing characteristics) to
     * the gatt server.
     * @param gattConnection The server connection on which the transaction will run.
     * @param service The service that needs to be added.
     */
    fun getAddServicesTransaction(gattConnection: GattServerConnection, service: BluetoothGattService) =
            AddGattServerServiceTransaction(gattConnection, GattState.ADD_SERVICE_SUCCESS, service)

}

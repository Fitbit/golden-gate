// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.bluetooth.fbgatt.rx.server

import com.fitbit.bluetooth.fbgatt.FitbitBluetoothDevice
import com.fitbit.bluetooth.fbgatt.GattServerConnection
import com.fitbit.bluetooth.fbgatt.GattState
import com.fitbit.bluetooth.fbgatt.rx.runTxReactive
import com.fitbit.bluetooth.fbgatt.tx.SendGattServerResponseTransaction
import io.reactivex.Completable
import timber.log.Timber

/**
 * Send acknowledgment for any change request to local GATT server sent from remote device that requires response
 */
class GattServerResponseSender(
        private val gattServerConnection: GattServerConnection,
        private val transactionProvider: SendGattServerResponseTransactionProvider = SendGattServerResponseTransactionProvider()
) {

    /**
     * Send acknowledgment to remote device that requires response
     *
     * @param successEndState end state for this transaction
     * @param device The remote device to send this response to
     * @param requestId The ID of the request that was received with the callback
     * @param status The status of the request to be sent to the remote devices
     * @param offset Value offset for partial read/write response
     * @param value The value of the attribute that was read/written (optional)
     */
    fun send(
            successEndState: GattState = GattState.SEND_SERVER_RESPONSE_SUCCESS,
            device: FitbitBluetoothDevice,
            requestId: Int,
            status: Int,
            offset: Int = 0,
            value: ByteArray? = null
    ): Completable {
        Timber.d("Sending response to GattServer for requestId: $requestId")
        return transactionProvider.provide(
                serverConnection = gattServerConnection,
                successEndState = successEndState,
                device = device,
                requestId = requestId,
                status = status,
                offset = offset,
                value = value
        ).runTxReactive(gattServerConnection).ignoreElement()
    }

}

class SendGattServerResponseTransactionProvider {

    fun provide(
            serverConnection: GattServerConnection,
            successEndState: GattState,
            device: FitbitBluetoothDevice,
            requestId: Int,
            status: Int,
            offset: Int,
            value: ByteArray? = null
    ): SendGattServerResponseTransaction {
        return SendGattServerResponseTransaction(
                serverConnection,
                successEndState,
                device,
                requestId,
                status,
                offset,
                value
        )
    }
}

class GattServerResponseSenderProvider {

    fun provide(gattServerConnection: GattServerConnection) =
            GattServerResponseSender(gattServerConnection)
}

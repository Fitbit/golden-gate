// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bt

import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGattServer
import android.bluetooth.BluetoothGattService
import com.fitbit.bluetooth.fbgatt.FitbitBluetoothDevice
import com.fitbit.bluetooth.fbgatt.FitbitGatt
import com.fitbit.bluetooth.fbgatt.GattConnection
import com.fitbit.bluetooth.fbgatt.GattServerConnection
import com.fitbit.bluetooth.fbgatt.GattServerTransaction
import com.fitbit.bluetooth.fbgatt.GattTransactionCallback
import com.fitbit.bluetooth.fbgatt.TransactionResult
import com.fitbit.bluetooth.fbgatt.TransactionResult.TransactionResultStatus
import org.mockito.kotlin.any
import org.mockito.kotlin.doReturn
import org.mockito.kotlin.eq
import org.mockito.kotlin.mock
import org.mockito.kotlin.whenever

const val mockBluetoothAddress = "00:43:A8:23:10:F0"
val mockBluetoothDevice = mock<BluetoothDevice> {
    on { address } doReturn mockBluetoothAddress
}
val mockFitbitBluetoothDevice = mock<FitbitBluetoothDevice> {
    on { btDevice } doReturn mockBluetoothDevice
}
val mockGattConnection = mock<GattConnection> {
    on { device } doReturn mockFitbitBluetoothDevice
}
val mockTransactionResult = mock<TransactionResult>()
val mockBluetoothGattServer = mock<BluetoothGattServer>()
val mockGattServerConnection = mock<GattServerConnection> {
    on { server } doReturn mockBluetoothGattServer
}
val mockBluetoothGattService = mock<BluetoothGattService>()
val mockFitbitGatt = mock<FitbitGatt> {
    on { server } doReturn mockGattServerConnection
    on { getConnection(mockBluetoothDevice) } doReturn mockGattConnection
}

fun GattServerTransaction.mockGattTransactionCompletion(result: TransactionResultStatus) {
    whenever(mockTransactionResult.resultStatus).thenReturn(result)
    mockGattTransactionCompletion()
}

fun GattServerTransaction.mockGattTransactionCompletion() {
    whenever(mockGattServerConnection.runTx(eq(this), any())).thenAnswer { invocation ->
        val callback = invocation.arguments[1] as GattTransactionCallback
        callback.onTransactionComplete(mockTransactionResult)
    }
}

fun FitbitGatt.mockDeviceKnown(isKnown: Boolean) {
    whenever(getConnectionForBluetoothAddress(any()))
            .thenReturn(if (isKnown) {
                mockGattConnection
            } else {
                null
            })
}

fun GattConnection.mockConnected(connected: Boolean) =
        whenever(isConnected).thenReturn(connected)

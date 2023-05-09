// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.bluetooth.fbgatt.rx

import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattDescriptor
import android.bluetooth.BluetoothGattServer
import android.bluetooth.BluetoothGattService
import android.content.Context
import com.fitbit.bluetooth.fbgatt.FitbitBluetoothDevice
import com.fitbit.bluetooth.fbgatt.FitbitGatt
import com.fitbit.bluetooth.fbgatt.GattClientTransaction
import com.fitbit.bluetooth.fbgatt.GattConnection
import com.fitbit.bluetooth.fbgatt.GattServerConnection
import com.fitbit.bluetooth.fbgatt.GattServerTransaction
import com.fitbit.bluetooth.fbgatt.GattTransaction
import com.fitbit.bluetooth.fbgatt.GattTransactionCallback
import com.fitbit.bluetooth.fbgatt.TransactionResult
import com.fitbit.bluetooth.fbgatt.TransactionResult.TransactionResultStatus
import org.mockito.kotlin.any
import org.mockito.kotlin.doReturn
import org.mockito.kotlin.eq
import org.mockito.kotlin.mock
import org.mockito.kotlin.whenever

val mockGattDescriptor = mock<BluetoothGattDescriptor>()
val mockGattCharacteristic = mock<BluetoothGattCharacteristic> {
    on { getDescriptor(any()) } doReturn mockGattDescriptor
}
val mockGattService = mock<BluetoothGattService>()
val mockBluetoothGatt = mock<BluetoothGatt>()
const val mockBluetoothAddress = "00:43:A8:23:10:F0"
val mockContext = mock<Context>()
val mockBluetoothDevice = mock<BluetoothDevice> {
    on { address } doReturn mockBluetoothAddress
}
val mockFitbitBluetoothDevice = mock<FitbitBluetoothDevice> {
    on { btDevice } doReturn mockBluetoothDevice
}
val mockGattConnection = mock<GattConnection> {
    on { device } doReturn mockFitbitBluetoothDevice
    on { gatt } doReturn mockBluetoothGatt
}
val mockTransactionResult = mock<TransactionResult>()
val mockBluetoothGattServer = mock<BluetoothGattServer>()
val mockGattServerConnection = mock<GattServerConnection> {
    on { server } doReturn mockBluetoothGattServer
}
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

fun GattClientTransaction.mockGattTransactionCompletion(result: TransactionResultStatus) {
    whenever(mockTransactionResult.resultStatus).thenReturn(result)
    mockGattTransactionCompletion()
}

fun GattClientTransaction.mockGattTransactionCompletion() {
    whenever(mockGattConnection.runTx(eq(this), any())).thenAnswer { invocation ->
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

// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.linkcontroller

import android.bluetooth.BluetoothGattServer
import com.fitbit.bluetooth.fbgatt.FitbitGatt
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

val mockTransactionResult = mock<TransactionResult>()
val mockBluetoothGattServer = mock<BluetoothGattServer>()
val mockGattServerConnection = mock<GattServerConnection> {
    on { server } doReturn mockBluetoothGattServer
}
val mockFitbitGatt = mock<FitbitGatt> {
    on { server } doReturn mockGattServerConnection
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

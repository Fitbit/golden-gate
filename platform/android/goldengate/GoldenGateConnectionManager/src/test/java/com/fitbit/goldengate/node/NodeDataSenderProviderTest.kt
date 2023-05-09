// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.node

import com.fitbit.bluetooth.fbgatt.FitbitGatt
import com.fitbit.goldengate.bt.gatt.server.services.gattlink.FitbitGattlinkService
import com.fitbit.goldengate.bt.gatt.server.services.gattlink.GattlinkService
import com.fitbit.goldengate.bt.mockBluetoothGattService
import com.fitbit.goldengate.bt.mockGattConnection
import org.mockito.kotlin.mock
import org.mockito.kotlin.whenever
import org.junit.Assert.assertTrue
import org.junit.Test

class NodeDataSenderProviderTest {

    private val fitbitGatt = mock<FitbitGatt>()
    private val provider = NodeDataSenderProvider(fitbitGatt)

    @Test
    fun shouldCreateRemoteSenderIfFitbitGattlinkOnRemoteDevice() {
        whenever(mockGattConnection.getRemoteGattService(FitbitGattlinkService.uuid)).thenReturn(mockBluetoothGattService)

        val receiver = provider.provide(mockGattConnection)

        assertTrue(receiver is RemoteGattlinkNodeDataSender )
    }

    @Test
    fun shouldCreateRemoteSenderIfGattlinkOnRemoteDevice() {
        whenever(mockGattConnection.getRemoteGattService(GattlinkService.uuid)).thenReturn(mockBluetoothGattService)

        val receiver = provider.provide(mockGattConnection)

        assertTrue(receiver is RemoteGattlinkNodeDataSender )
    }

    @Test
    fun shouldCreateLocalReceiverIfGattlinkNotOnRemoteDevice() {
        whenever(mockGattConnection.getRemoteGattService(FitbitGattlinkService.uuid)).thenReturn(null)
        whenever(mockGattConnection.getRemoteGattService(GattlinkService.uuid)).thenReturn(null)

        val receiver = provider.provide(mockGattConnection)

        assertTrue(receiver is LocalGattlinkNodeDataSender )
    }

}

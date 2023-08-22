// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.node

import com.fitbit.goldengate.bt.gatt.server.services.gattlink.GattlinkService
import com.fitbit.goldengate.bt.mockBluetoothGattService
import com.fitbit.goldengate.bt.mockGattConnection
import org.mockito.kotlin.whenever
import org.junit.Assert.*
import org.junit.Test

class NodeDataReceiverProviderTest {

    private val provider = NodeDataReceiverProvider()

    @Test
    fun shouldCreateRemoteReceiverIfGattlinkOnRemoteDevice() {
        whenever(mockGattConnection.getRemoteGattService(GattlinkService.uuid)).thenReturn(mockBluetoothGattService)

        val receiver = provider.provide(mockGattConnection)

        assertTrue(receiver is RemoteGattlinkNodeDataReceiver )
    }

    @Test
    fun shouldCreateLocalReceiverIfGattlinkNotOnRemoteDevice() {
        whenever(mockGattConnection.getRemoteGattService(GattlinkService.uuid)).thenReturn(null)

        val receiver = provider.provide(mockGattConnection)

        assertTrue(receiver is LocalGattlinkNodeDataReceiver )
    }

}

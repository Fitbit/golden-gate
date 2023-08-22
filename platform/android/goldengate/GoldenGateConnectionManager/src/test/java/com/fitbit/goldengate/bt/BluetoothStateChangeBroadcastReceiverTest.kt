// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bt

import android.bluetooth.BluetoothAdapter
import android.content.Context
import android.content.Intent
import org.mockito.kotlin.mock
import org.junit.After
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(manifest = Config.NONE)
class BluetoothStateChangeBroadcastReceiverTest {

    private val mockContext = mock<Context>()

    private val receiver = BluetoothStateChangeBroadcastReceiver()

    @After
    fun tearDown() {
        receiver.unregister(mockContext)
    }

    @Test
    fun shouldEmitStateChangeOnRegisteredListener() {

        mockStateChangeBroadcastReceiveCall(BluetoothAdapter.STATE_ON)

        receiver.register(mockContext)
                .test()
                .assertValue { it == BluetoothState.ON }
    }

    @Test
    fun shouldEmitUnknownStateForUnknownValue() {

        mockStateChangeBroadcastReceiveCall(4341)

        receiver.register(mockContext)
                .test()
                .assertValue { it == BluetoothState.UNKNOWN }
    }

    private fun mockStateChangeBroadcastReceiveCall(state: Int) {
        val intent = Intent(BluetoothAdapter.ACTION_STATE_CHANGED)
        intent.putExtra(BluetoothAdapter.EXTRA_STATE, state)
        receiver.onReceive(mockContext, intent)
    }
}

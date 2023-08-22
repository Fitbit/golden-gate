// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.bluetooth.fbgatt.rx

import android.bluetooth.BluetoothDevice
import com.fitbit.bluetooth.fbgatt.FitbitBluetoothDevice
import com.fitbit.bluetooth.fbgatt.GattConnection
import com.fitbit.bluetooth.fbgatt.rx.client.listeners.GattClientConnectionChangeListener
import com.fitbit.bluetooth.fbgatt.rx.server.listeners.GattServerConnectionChangeListener
import org.mockito.kotlin.doReturn
import org.mockito.kotlin.mock
import org.mockito.kotlin.whenever
import io.reactivex.Observable
import org.junit.Test

class PeripheralConnectionChangeListenerTest {

    private val mockBluetoothDevice = mock<BluetoothDevice>()

    private val mockFitbitDevice = mock<FitbitBluetoothDevice> {
        on { btDevice } doReturn mockBluetoothDevice
    }

    private val mockGattConnection = mock<GattConnection>() {
        on { device } doReturn mockFitbitDevice
    }

    private val mockGattServerConnectionChangeListener = mock<GattServerConnectionChangeListener>()
    private val mockGattClientConnectionChangeListener = mock<GattClientConnectionChangeListener>()

    private val listener = PeripheralConnectionChangeListener(
        mockGattServerConnectionChangeListener,
        mockGattClientConnectionChangeListener
    )

    @Test
    fun shouldEmmitDisconnectIfGattServerIsDisconnected() {
        whenever(mockGattServerConnectionChangeListener.getDataObservable(mockGattConnection.device.btDevice)).thenReturn(
            Observable.just(PeripheralConnectionStatus.DISCONNECTED)
        )
        whenever(mockGattClientConnectionChangeListener.register(mockGattConnection)).thenReturn(
            Observable.never()
        )

        listener.register(mockGattConnection)
            .test()
            .assertValue { it == PeripheralConnectionStatus.DISCONNECTED }
    }

    @Test
    fun shouldEmmitDisconnectIfGattClientIsDisconnected() {
        whenever(mockGattServerConnectionChangeListener.getDataObservable(mockGattConnection.device.btDevice)).thenReturn(
            Observable.never()
        )
        whenever(mockGattClientConnectionChangeListener.register(mockGattConnection)).thenReturn(
            Observable.just(PeripheralConnectionStatus.DISCONNECTED)
        )

        listener.register(mockGattConnection)
            .test()
            .assertValue { it == PeripheralConnectionStatus.DISCONNECTED }
    }

    @Test
    fun shouldEmmitDisconnectIfGattClientAndServerIsDisconnected() {
        whenever(mockGattServerConnectionChangeListener.getDataObservable(mockGattConnection.device.btDevice)).thenReturn(
            Observable.just(PeripheralConnectionStatus.DISCONNECTED)
        )
        whenever(mockGattClientConnectionChangeListener.register(mockGattConnection)).thenReturn(
            Observable.just(PeripheralConnectionStatus.DISCONNECTED)
        )

        listener.register(mockGattConnection)
            .test()
            .assertValue { it == PeripheralConnectionStatus.DISCONNECTED }
    }
}

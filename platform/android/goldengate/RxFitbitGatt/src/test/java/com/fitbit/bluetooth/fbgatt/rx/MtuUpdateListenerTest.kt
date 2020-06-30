package com.fitbit.bluetooth.fbgatt.rx

import android.bluetooth.BluetoothDevice
import com.fitbit.bluetooth.fbgatt.FitbitBluetoothDevice
import com.fitbit.bluetooth.fbgatt.GattConnection
import com.fitbit.bluetooth.fbgatt.rx.client.listeners.GattClientMtuChangeListener
import com.fitbit.bluetooth.fbgatt.rx.server.listeners.GattServerMtuChangeListener
import com.nhaarman.mockitokotlin2.doReturn
import com.nhaarman.mockitokotlin2.mock
import com.nhaarman.mockitokotlin2.whenever
import io.reactivex.Observable
import org.junit.Test

class MtuUpdateListenerTest {
    private val mockBluetoothDevice = mock<BluetoothDevice>()

    private val mockFitbitDevice = mock<FitbitBluetoothDevice> {
        on { btDevice } doReturn mockBluetoothDevice
    }

    private val mockGattConnection = mock<GattConnection>() {
        on { device } doReturn mockFitbitDevice
    }

    private val serverMtuChangeListener: GattServerMtuChangeListener = mock<GattServerMtuChangeListener>()
    private val clientMtuChangeListener: GattClientMtuChangeListener = mock<GattClientMtuChangeListener>()

    private val listener = MtuUpdateListener(
        serverMtuChangeListener,
        clientMtuChangeListener
    )

    @Test
    fun `should emit mtu value if new value comes from gatt server callback`() {
        val newMtu = 100
        whenever(serverMtuChangeListener.getMtuSizeObservable(mockBluetoothDevice)).thenReturn(
            Observable.just(newMtu)
        )

        whenever(clientMtuChangeListener.register(mockGattConnection)).thenReturn(
            Observable.never()
        )

        listener.register(mockGattConnection)
            .test()
            .assertValue { it == newMtu }
    }

    @Test
    fun `should emit mtu value if new value comes from gatt client callback`() {
        val newMtu = 100
        whenever(serverMtuChangeListener.getMtuSizeObservable(mockBluetoothDevice)).thenReturn(
            Observable.never()
        )

        whenever(clientMtuChangeListener.register(mockGattConnection)).thenReturn(
            Observable.just(newMtu)
        )

        listener.register(mockGattConnection)
            .test()
            .assertValue { it == newMtu }
    }

    @Test
    fun `should filter out duplication`() {
        val newMtu = 100
        whenever(serverMtuChangeListener.getMtuSizeObservable(mockBluetoothDevice)).thenReturn(
            Observable.just(newMtu)
        )

        whenever(clientMtuChangeListener.register(mockGattConnection)).thenReturn(
            Observable.just(newMtu)
        )

        listener.register(mockGattConnection)
            .test()
            .assertValueCount(1)
            .assertValue { it == newMtu }
    }
}

// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.bluetooth.fbgatt.rx.client

import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattService
import com.fitbit.bluetooth.fbgatt.rx.GattCharacteristicException
import com.fitbit.bluetooth.fbgatt.rx.GattServiceNotFoundException
import org.mockito.kotlin.doReturn
import org.mockito.kotlin.mock
import org.mockito.kotlin.never
import org.mockito.kotlin.verify
import org.mockito.kotlin.whenever
import io.reactivex.Maybe
import io.reactivex.Single
import org.junit.Test
import java.util.UUID

class PeerGattServiceSubscriberTest {

    private val mockServiceId = UUID.fromString("FFFFFFFF-FFFF-FFFF-FFFF-FFFFFFFFFFF1")
    private val mockCharacteristicId = UUID.fromString("FFFFFFFF-FFFF-FFFF-FFFF-FFFFFFFFFFF2")

    private val mockBluetoothGattService = mock<BluetoothGattService>()
    private val mockBluetoothGattCharacteristic = mock<BluetoothGattCharacteristic>()
    private val mockPeripheral = mock<BitGattPeer> {
        on { setupNotifications(mockBluetoothGattCharacteristic, true) } doReturn Single.just(mockBluetoothGattCharacteristic)
    }

    private val subscriber = PeerGattServiceSubscriber()

    @Test
    fun shouldSubscribeIfServiceAndCharacteristicIsNonNull() {
        whenever(mockPeripheral.getService(mockServiceId)).thenReturn(Maybe.just(mockBluetoothGattService))
        whenever(mockBluetoothGattService.getCharacteristic(mockCharacteristicId)).thenReturn(mockBluetoothGattCharacteristic)

        subscriber.subscribe(mockPeripheral, mockServiceId, mockCharacteristicId)
                .test()
                .assertComplete()

        verify(mockPeripheral).setupNotifications(mockBluetoothGattCharacteristic, true)
    }

    @Test
    fun shouldEmitErrorIfServiceIsNull() {
        whenever(mockPeripheral.getService(mockServiceId)).thenReturn(Maybe.empty())

        subscriber.subscribe(mockPeripheral, mockServiceId, mockCharacteristicId)
                .test()
                .assertError { it is GattServiceNotFoundException }

        verify(mockPeripheral, never()).setupNotifications(mockBluetoothGattCharacteristic, true)
    }

    @Test
    fun shouldEmitErrorIfCharacteristicIsNull() {
        whenever(mockPeripheral.getService(mockServiceId)).thenReturn(Maybe.just(mockBluetoothGattService))
        whenever(mockBluetoothGattService.getCharacteristic(mockCharacteristicId)).thenReturn(null)

        subscriber.subscribe(mockPeripheral, mockServiceId, mockCharacteristicId)
                .test()
                .assertError { it is GattCharacteristicException }

        verify(mockPeripheral, never()).setupNotifications(mockBluetoothGattCharacteristic, true)
    }
}

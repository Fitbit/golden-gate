// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.linkcontroller

import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattService
import com.fitbit.bluetooth.fbgatt.FitbitBluetoothDevice
import com.fitbit.bluetooth.fbgatt.GattConnection
import com.fitbit.bluetooth.fbgatt.rx.client.BitGattPeer
import com.fitbit.bluetooth.fbgatt.rx.client.GattCharacteristicReader
import com.fitbit.bluetooth.fbgatt.rx.client.PeerGattServiceSubscriber
import com.fitbit.bluetooth.fbgatt.rx.client.listeners.GattClientCharacteristicChangeListener
import com.fitbit.bluetooth.fbgatt.rx.server.GattCharacteristicNotifier
import com.fitbit.linkcontroller.services.configuration.ClientPreferredConnectionConfigurationCharacteristic
import com.fitbit.linkcontroller.services.configuration.ClientPreferredConnectionModeCharacteristic
import com.fitbit.linkcontroller.services.configuration.GattCharacteristicSubscriptionStatus
import com.fitbit.linkcontroller.services.configuration.LinkConfigurationService
import com.fitbit.linkcontroller.services.configuration.PreferredConnectionConfiguration
import com.fitbit.linkcontroller.services.configuration.PreferredConnectionMode
import com.fitbit.linkcontroller.services.status.CurrentConnectionConfiguration
import com.fitbit.linkcontroller.services.status.CurrentConnectionStatus
import com.fitbit.linkcontroller.services.status.LinkStatusService
import com.nhaarman.mockitokotlin2.*
import io.reactivex.Completable
import io.reactivex.Maybe
import io.reactivex.Observable
import io.reactivex.Single
import io.reactivex.subjects.BehaviorSubject
import org.junit.Test
import java.nio.BufferUnderflowException

class LinkControllerTest {

    private val mockGattClientCharacteristicChangeListener =
        mock<GattClientCharacteristicChangeListener>()
    private val mockGattCharacteristicReader = mock<GattCharacteristicReader>()
    private val mockFitbitBluetoothDevice = mock<FitbitBluetoothDevice>()
    private val mockGattConnection = mock<GattConnection>()
    private val mockBluetoothGattCharacteristic = mock<BluetoothGattCharacteristic>()
    private val mockBluetoothGattService = mock<BluetoothGattService> {
        on { getCharacteristic(any()) } doReturn mockBluetoothGattCharacteristic
    }
    private val mockLinkConfigurationCharacteristicNotifier =
        mock<GattCharacteristicNotifier> {
            on { notify(any(), any(), any()) } doReturn Completable.complete()
        }
    private val mockRxBlePeripheral = mock<BitGattPeer> {
        on { fitbitDevice } doReturn mockFitbitBluetoothDevice
        on { getService(any()) } doReturn Maybe.just(mockBluetoothGattService)
    }
    private val mockPeripheralServiceSubscriber = mock<PeerGattServiceSubscriber>()
    private val linkConfigurationSubscriptionObservable =
        BehaviorSubject.createDefault<GattCharacteristicSubscriptionStatus>(
            GattCharacteristicSubscriptionStatus.ENABLED
        )

    private val linkController = LinkController(
        mockGattConnection,
        linkConfigurationSubscriptionObservable,
        linkConfigurationSubscriptionObservable,
        linkConfigurationSubscriptionObservable,
        mockLinkConfigurationCharacteristicNotifier,
        mockRxBlePeripheral,
        mockGattCharacteristicReader,
        mockGattClientCharacteristicChangeListener,
        mockPeripheralServiceSubscriber
    )

    @Test
    fun getCurrentLinkConfigurationTest() {
        val array = ByteArray(9)
        val configuration = CurrentConnectionConfiguration.parseFromByteArray(array)
        whenever(mockGattCharacteristicReader.read(any(), any())).thenReturn(
            Single.just(array)
        )
        linkController.getCurrentConnectionConfiguration().test().assertResult(configuration)
    }

    @Test
    fun getCurrentLinkConnectionStatusTest() {
        val array = ByteArray(7)
        val status = CurrentConnectionStatus.parseFromByteArray(array)
        whenever(mockGattCharacteristicReader.read(any(), any())).thenReturn(
            Single.just(array)
        )
        linkController.getCurrentConnectionStatus().test().assertResult(status)
    }

    @Test
    fun getErrorWhenBadReadCharacteristicTest() {
        val array = ByteArray(1)
        whenever(mockGattCharacteristicReader.read(any(), any())).thenReturn(
            Single.just(array)
        )
        linkController.getCurrentConnectionStatus().test()
            .assertFailure(BufferUnderflowException::class.java)
    }

    @Test
    fun setPreferredConnectionModeTest() {
        linkController.setPreferredConnectionMode(PreferredConnectionMode.FAST).test()
            .assertComplete()
        verify(mockLinkConfigurationCharacteristicNotifier).notify(
            LinkConfigurationService.uuid,
            ClientPreferredConnectionModeCharacteristic.uuid,
            PreferredConnectionMode.FAST.toByteArray()
        )
    }

    @Test
    fun setPreferredConnectionConfigurationTest() {
        val connectionConfiguration = PreferredConnectionConfiguration.Builder().build()
        linkController.setPreferredConnectionConfiguration(connectionConfiguration).test()
            .assertComplete()
        verify(mockLinkConfigurationCharacteristicNotifier).notify(
            LinkConfigurationService.uuid,
            ClientPreferredConnectionConfigurationCharacteristic.uuid,
            connectionConfiguration.toByteArray()
        )
    }

    @Test
    fun observeCurrentConfigurationTest() {
        whenever(
            mockGattClientCharacteristicChangeListener.register(
                mockGattConnection,
                LinkStatusService.currentConfigurationUuid
            )
        )
            .thenReturn(
                Observable.just(
                    ByteArray(9),
                    ByteArray(9),
                    ByteArray(9)
                )
            )
        linkController.observeCurrentConnectionConfiguration()
            .test()
            .assertValueCount(3)
    }

    @Test
    fun observeCurrentConnectionStatusTest() {
        whenever(
            mockGattClientCharacteristicChangeListener.register(
                mockGattConnection,
                LinkStatusService.currentConnectionModeUuid
            )
        )
            .thenReturn(
                Observable.just(
                    ByteArray(9),
                    ByteArray(9),
                    ByteArray(9)
                )
            )
        linkController.observeCurrentConnectionStatus()
            .test()
            .assertValueCount(3)
    }

    @Test
    fun subscribeTest() {
        whenever(mockPeripheralServiceSubscriber.subscribe(any(), any(), any(), any())).thenReturn(
            Completable.complete()
        )

        linkController.subscribeToCurrentConnectionConfigurationNotifications().test()
            .assertComplete()
        verify(mockPeripheralServiceSubscriber).subscribe(
            mockRxBlePeripheral,
            LinkStatusService.uuid,
            LinkStatusService.currentConfigurationUuid
        )

        linkController.subscribeToCurrentConnectionStatusNotifications().test().assertComplete()
        verify(mockPeripheralServiceSubscriber).subscribe(
            mockRxBlePeripheral,
            LinkStatusService.uuid,
            LinkStatusService.currentConnectionModeUuid
        )
    }

    @Test
    fun `fail SetPreferredConnectionMode when not subscribed Test`() {
        linkConfigurationSubscriptionObservable.onNext(GattCharacteristicSubscriptionStatus.DISABLED)
        linkController.setPreferredConnectionMode(PreferredConnectionMode.FAST).test()
            .assertError(IllegalStateException::class.java)
    }

    @Test
    fun `fail SetPreferredConnectionConfiguration when not subscribed Test`() {
        linkConfigurationSubscriptionObservable.onNext(GattCharacteristicSubscriptionStatus.DISABLED)
        val connectionConfiguration = PreferredConnectionConfiguration.Builder().build()
        linkController.setPreferredConnectionConfiguration(connectionConfiguration).test()
            .assertError(IllegalStateException::class.java)
    }
}

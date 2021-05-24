// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.linkcontroller

import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattService
import com.fitbit.bluetooth.fbgatt.FitbitBluetoothDevice
import com.fitbit.bluetooth.fbgatt.FitbitGatt
import com.fitbit.bluetooth.fbgatt.GattConnection
import com.fitbit.bluetooth.fbgatt.rx.client.BitGattPeer
import com.fitbit.bluetooth.fbgatt.rx.client.GattCharacteristicReader
import com.fitbit.bluetooth.fbgatt.rx.client.PeerGattServiceSubscriber
import com.fitbit.bluetooth.fbgatt.rx.client.listeners.GattClientCharacteristicChangeListener
import com.fitbit.bluetooth.fbgatt.rx.server.GattCharacteristicNotifier
import com.fitbit.linkcontroller.services.configuration.ClientPreferredConnectionConfigurationCharacteristic
import com.fitbit.linkcontroller.services.configuration.ClientPreferredConnectionModeCharacteristic
import com.fitbit.linkcontroller.services.configuration.GattCharacteristicSubscriptionStatus
import com.fitbit.linkcontroller.services.configuration.LinkConfigurationPeerRequestListener
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
import kotlin.test.assertEquals

class LinkControllerTest {

    private val testAddress = "1"
    private val mockGattClientCharacteristicChangeListener =
        mock<GattClientCharacteristicChangeListener>()
    private val mockGattCharacteristicReader = mock<GattCharacteristicReader>()
    private val mockBluetoothDevice = mock<BluetoothDevice> {
        on {  address } doReturn testAddress
    }
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
        BehaviorSubject.createDefault(
            GattCharacteristicSubscriptionStatus.ENABLED
        )
    private val mockFitbitGatt = mock<FitbitGatt> {
        on { getConnectionForBluetoothAddress(testAddress) } doReturn mockGattConnection
    }

    private val linkController = LinkController(
        mockBluetoothDevice,
        linkConfigurationSubscriptionObservable,
        linkConfigurationSubscriptionObservable,
        linkConfigurationSubscriptionObservable,
        mockLinkConfigurationCharacteristicNotifier,
        { mockRxBlePeripheral } ,
        { mockGattCharacteristicReader },
        { mockGattClientCharacteristicChangeListener },
        mockPeripheralServiceSubscriber,
        mockFitbitGatt
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

    @Test
    fun `verify LinkConfigurationPeerRequestListeners register and unregister operations`() {
        val listener1 = mock<LinkConfigurationPeerRequestListener>()
        val listener2 = mock<LinkConfigurationPeerRequestListener>()
        val listener3 = mock<LinkConfigurationPeerRequestListener>()

        linkController.unregisterLinkConfigurationPeerRequestListener(listener1)
        assertEquals(linkController.getLinkConfigurationPeerRequestListeners().size, 0)

        linkController.registerLinkConfigurationPeerRequestListener(listener1)
        linkController.registerLinkConfigurationPeerRequestListener(listener1)
        assertEquals(linkController.getLinkConfigurationPeerRequestListeners().size, 1)

        linkController.registerLinkConfigurationPeerRequestListener(listener2)
        linkController.registerLinkConfigurationPeerRequestListener(listener3)
        assertEquals(linkController.getLinkConfigurationPeerRequestListeners().size, 3)

        linkController.unregisterLinkConfigurationPeerRequestListener(listener1)
        linkController.unregisterLinkConfigurationPeerRequestListener(listener1)
        assertEquals(linkController.getLinkConfigurationPeerRequestListeners().size, 2)

        linkController.unregisterLinkConfigurationPeerRequestListener(listener2)
        linkController.unregisterLinkConfigurationPeerRequestListener(listener3)
        assertEquals(linkController.getLinkConfigurationPeerRequestListeners().size, 0)
    }

    @Test
    fun shouldResetToSlowModeOnDisconnection() {
        assertEquals(PreferredConnectionMode.SLOW, linkController.getPreferredConnectionMode())
        linkController.setPreferredConnectionMode(PreferredConnectionMode.FAST)
        assertEquals(PreferredConnectionMode.FAST, linkController.getPreferredConnectionMode())
        linkController.handleDisconnection()
        assertEquals(PreferredConnectionMode.SLOW, linkController.getPreferredConnectionMode())
    }

    @Test
    fun shouldResetToDefaultConfigurationOnDisconnection() {
        val defaultConfig = PreferredConnectionConfiguration()
        assertEquals(defaultConfig, linkController.getPreferredConnectionConfiguration())

        val config = PreferredConnectionConfiguration.Builder()
            .setFastModeConfig(1000f, 1000f, 0, 6000)
            .build()
        linkController.setPreferredConnectionConfiguration(config)
        assertEquals(config, linkController.getPreferredConnectionConfiguration())

        linkController.handleDisconnection()
        assertEquals(defaultConfig, linkController.getPreferredConnectionConfiguration())
    }
}

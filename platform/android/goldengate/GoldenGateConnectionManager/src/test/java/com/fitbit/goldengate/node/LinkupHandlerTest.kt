// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.node

import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattService
import com.fitbit.bluetooth.fbgatt.rx.GattCharacteristicSubscriptionStatus
import com.fitbit.bluetooth.fbgatt.rx.GattServiceNotFoundException
import com.fitbit.bluetooth.fbgatt.rx.client.BitGattPeripheral
import com.fitbit.bluetooth.fbgatt.rx.client.GattServiceRefresher
import com.fitbit.bluetooth.fbgatt.rx.client.PeripheralServiceSubscriber
import com.fitbit.goldengate.bt.gatt.client.services.GattDatabaseValidator
import com.fitbit.goldengate.bt.gatt.client.services.GattDatabaseValidatorException
import com.fitbit.goldengate.bt.gatt.server.services.gattlink.GattlinkService
import com.fitbit.goldengate.bt.gatt.server.services.gattlink.TransmitCharacteristic
import com.fitbit.goldengate.bt.gatt.server.services.gattlink.listeners.TransmitCharacteristicSubscriptionListener
import com.fitbit.goldengate.bt.mockBluetoothDevice
import com.nhaarman.mockitokotlin2.doReturn
import com.nhaarman.mockitokotlin2.mock
import com.nhaarman.mockitokotlin2.never
import com.nhaarman.mockitokotlin2.verify
import com.nhaarman.mockitokotlin2.whenever
import io.reactivex.Completable
import io.reactivex.Observable
import io.reactivex.Single
import io.reactivex.plugins.RxJavaPlugins
import io.reactivex.schedulers.TestScheduler
import org.junit.After
import org.junit.Before
import org.junit.Test
import org.mockito.Mockito.times
import java.util.concurrent.TimeUnit
import java.util.concurrent.TimeoutException


class LinkupHandlerTest {

    private val mockFitbitBluetoothDevice = com.fitbit.goldengate.bt.mockFitbitBluetoothDevice
    private val mockBluetoothGatt = mock<BluetoothGatt>()

    private val mockPeripheral = mock<BitGattPeripheral> {
        on { fitbitDevice } doReturn mockFitbitBluetoothDevice
        on { connect() } doReturn Single.just(mockBluetoothGatt)
    }

    private val mockPeripheralServiceSubscriber = mock<PeripheralServiceSubscriber>()
    private val mockGattDatabaseValidator = mock<GattDatabaseValidator>()
    private val mockGattServiceRefresher = mock<GattServiceRefresher> {
        on { refresh(mockPeripheral.gattConnection) } doReturn Completable.complete()
    }

    private val linkUpTimeoutSeconds = 60L

    private val testScheduler = TestScheduler()

    private val handler = LinkupHandler(
        mockPeripheralServiceSubscriber,
        mockGattDatabaseValidator,
        mockGattServiceRefresher,
        linkUpTimeoutSeconds = linkUpTimeoutSeconds,
        timeoutScheduler = testScheduler
    )

    @Before
    fun setup() {
        RxJavaPlugins.setComputationSchedulerHandler { testScheduler }
    }

    @After
    fun tearDown() {
        RxJavaPlugins.reset()
    }

    @Test
    fun shouldSubscribeToRemoteGattlinkService() {
        mockDiscoverServicesSuccess()
        mockGattDatabaseValidatorSuccess()
        mockRemoteGattlinkSubscriptionSuccess()

        val testObserver = handler.link(mockPeripheral)
            .test()

        verify(mockPeripheral, times(1)).discoverServices()
        verify(mockPeripheralServiceSubscriber)
            .subscribe(mockPeripheral, GattlinkService.uuid, TransmitCharacteristic.uuid)

        testObserver
            .assertComplete()
            .dispose()
    }

    @Test
    fun shouldRefreshServicesIfServiceDiscoveryFails() {
        mockDiscoverServicesFailure()
        mockGattDatabaseValidatorFailure()

        val testObserver = handler.link(mockPeripheral)
            .test()

        testScheduler.advanceTimeBy(1, TimeUnit.SECONDS)
        verify(mockPeripheral, times(1)).discoverServices()

        testScheduler.advanceTimeBy(2, TimeUnit.SECONDS)
        verify(mockPeripheral, times(2)).discoverServices()

        testScheduler.advanceTimeBy(4, TimeUnit.SECONDS)
        verify(mockPeripheral, times(3)).discoverServices()

        testScheduler.advanceTimeBy(6, TimeUnit.SECONDS)
        verify(mockPeripheral, times(5)).discoverServices()
        verify(mockGattServiceRefresher, times(1)).refresh(mockPeripheral.gattConnection)

        verify(mockPeripheralServiceSubscriber, never())
            .subscribe(mockPeripheral, GattlinkService.uuid, TransmitCharacteristic.uuid)

        testObserver
            .assertFailureAndMessage(RuntimeException::class.java, "Failed")
            .dispose()
    }

    @Test
    fun shouldSubscribeToRemoteGattlinkServiceIfServiceDiscoveryRetryWorks() {
        mockDiscoverServicesRetrySuccess()
        mockGattDatabaseValidatorSuccess()
        mockRemoteGattlinkSubscriptionSuccess()

        val testObserver = handler.link(mockPeripheral)
            .test()

        testScheduler.advanceTimeBy(1, TimeUnit.SECONDS)
        verify(mockPeripheral, times(1)).discoverServices()

        testScheduler.advanceTimeBy(10, TimeUnit.SECONDS)
        verify(mockPeripheral, times(2)).discoverServices()

        verify(mockPeripheralServiceSubscriber)
            .subscribe(mockPeripheral, GattlinkService.uuid, TransmitCharacteristic.uuid)

        testObserver
            .assertComplete()
            .dispose()
    }

    @Test
    fun shouldFailIfRemoteGattlinkServiceNotFound() {
        mockDiscoverServicesSuccess()
        mockGattDatabaseValidatorSuccess()
        mockRemoteGattlinkServiceNotFound()

        val testObserver = handler.link(mockPeripheral)
            .test()

        verify(mockPeripheral, times(1)).discoverServices()
        verify(mockPeripheralServiceSubscriber)
            .subscribe(mockPeripheral, GattlinkService.uuid, TransmitCharacteristic.uuid)

        testObserver
            .assertFailure(GattServiceNotFoundException::class.java)
            .dispose()
    }

    private fun mockDiscoverServicesSuccess(services: List<BluetoothGattService> = emptyList()) =
        whenever(mockPeripheral.discoverServices()).thenReturn(Single.just(services))

    private fun mockDiscoverServicesRetrySuccess(services: List<BluetoothGattService> = emptyList()) =
        whenever(mockPeripheral.discoverServices()).thenReturn(
            Single.error(RuntimeException("Failed")),
            Single.just(services)
        )

    private fun mockDiscoverServicesFailure() =
        whenever(mockPeripheral.discoverServices()).thenReturn(
            Single.error(RuntimeException("Failed")))

    private fun mockRemoteGattlinkSubscriptionSuccess() =
        whenever(mockPeripheralServiceSubscriber.subscribe(
            mockPeripheral,
            GattlinkService.uuid,
            TransmitCharacteristic.uuid
        )).thenReturn(Completable.complete())

    private fun mockRemoteGattlinkServiceNotFound() =
        whenever(mockPeripheralServiceSubscriber.subscribe(
            mockPeripheral,
            GattlinkService.uuid,
            TransmitCharacteristic.uuid
        )).thenReturn(Completable.error(GattServiceNotFoundException(GattlinkService.uuid)))

    private fun mockGattDatabaseValidatorSuccess() =
        whenever(mockGattDatabaseValidator.validate(mockPeripheral)).thenReturn(Completable.complete())

    private fun mockGattDatabaseValidatorFailure() =
        whenever(mockGattDatabaseValidator.validate(mockPeripheral)).thenReturn(
            Completable.error(GattDatabaseValidatorException())
        )

}

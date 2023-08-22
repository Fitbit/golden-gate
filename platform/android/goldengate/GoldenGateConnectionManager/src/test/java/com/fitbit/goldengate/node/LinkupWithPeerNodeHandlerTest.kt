// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.node

import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattService
import com.fitbit.bluetooth.fbgatt.GattConnection
import com.fitbit.bluetooth.fbgatt.rx.GattServiceNotFoundException
import com.fitbit.bluetooth.fbgatt.rx.client.BitGattPeer
import com.fitbit.bluetooth.fbgatt.rx.client.GattServiceDiscoverer
import com.fitbit.bluetooth.fbgatt.rx.client.GattServiceRefresher
import com.fitbit.bluetooth.fbgatt.rx.client.PeerGattServiceSubscriber
import com.fitbit.goldengate.bt.gatt.client.services.GattDatabaseValidator
import com.fitbit.goldengate.bt.gatt.client.services.GattDatabaseValidatorException
import com.fitbit.goldengate.bt.gatt.server.services.gattlink.FitbitGattlinkService
import com.fitbit.goldengate.bt.gatt.server.services.gattlink.GattlinkService
import com.fitbit.goldengate.bt.gatt.server.services.gattlink.TransmitCharacteristic
import org.mockito.kotlin.doReturn
import org.mockito.kotlin.mock
import org.mockito.kotlin.never
import org.mockito.kotlin.verify
import org.mockito.kotlin.whenever
import io.reactivex.Completable
import io.reactivex.Single
import io.reactivex.plugins.RxJavaPlugins
import io.reactivex.schedulers.TestScheduler
import org.junit.After
import org.junit.Before
import org.junit.Test
import org.mockito.Mockito.times
import java.util.concurrent.TimeUnit


class LinkupWithPeerNodeHandlerTest {

    private val mockFitbitBluetoothDevice = com.fitbit.goldengate.bt.mockFitbitBluetoothDevice
    private val mockBluetoothGattService = mock<BluetoothGattService>()
    private val mockBluetoothGatt = mock<BluetoothGatt>()
    private val mockGattConnection = mock<GattConnection>() {
        on { getRemoteGattService(FitbitGattlinkService.uuid) } doReturn null
        on { getRemoteGattService(GattlinkService.uuid) } doReturn mockBluetoothGattService
    }

    private val mockPeripheral = mock<BitGattPeer> {
        on { fitbitDevice } doReturn mockFitbitBluetoothDevice
        on { connect() } doReturn Single.just(mockBluetoothGatt)
        on { gattConnection } doReturn mockGattConnection
    }

    private val mockPeerGattServiceSubscriber = mock<PeerGattServiceSubscriber>()
    private val mockGattDatabaseValidator = mock<GattDatabaseValidator>()
    private val mockGattServiceRefresher = mock<GattServiceRefresher> {
        on { refresh(mockPeripheral.gattConnection) } doReturn Completable.complete()
    }
    private val mockGattServiceDiscoverer = mock<GattServiceDiscoverer>()

    private val linkUpTimeoutSeconds = 60L

    private val testScheduler = TestScheduler()

    private val handler = LinkupWithPeerNodeHandler(
        mockPeerGattServiceSubscriber,
        mockGattDatabaseValidator,
        mockGattServiceRefresher,
        mockGattServiceDiscoverer,
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

        verify(mockGattServiceDiscoverer, times(1)).discover(mockGattConnection)
        verify(mockPeerGattServiceSubscriber)
            .subscribe(mockPeripheral, GattlinkService.uuid, TransmitCharacteristic.uuid)

        testObserver
            .assertComplete()
            .dispose()
    }

    @Test
    fun shouldSubscribeToRemoteFitbitGattlinkService() {
        mockDiscoverServicesSuccess()
        mockGattDatabaseValidatorSuccess()
        mockRemoteFitbitGattlinkSubscriptionSuccess()
        whenever(mockPeripheral.gattConnection.getRemoteGattService(GattlinkService.uuid)).thenReturn(null)

        val testObserver = handler.link(mockPeripheral)
            .test()

        verify(mockGattServiceDiscoverer, times(1)).discover(mockGattConnection)
        verify(mockPeerGattServiceSubscriber)
            .subscribe(mockPeripheral, FitbitGattlinkService.uuid, TransmitCharacteristic.uuid)

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

        // 1st retry after 1 sec
        testScheduler.advanceTimeBy(1, TimeUnit.SECONDS)
        verify(mockGattServiceDiscoverer, times(1)).discover(mockGattConnection)

        // 1st retry after 2 sec
        testScheduler.advanceTimeBy(2, TimeUnit.SECONDS)
        verify(mockGattServiceDiscoverer, times(2)).discover(mockGattConnection)

        // 1st retry after 4 sec
        testScheduler.advanceTimeBy(4, TimeUnit.SECONDS)
        verify(mockGattServiceDiscoverer, times(3)).discover(mockGattConnection)

        // 1st retry after 6 sec + 2 sec for delay between refresh+discover call
        testScheduler.advanceTimeBy(6 + GATT_SERVICE_REFRESH_DELAY_IN_SEC, TimeUnit.SECONDS)
        verify(mockGattServiceDiscoverer, times(5)).discover(mockGattConnection)
        verify(mockGattServiceRefresher, times(1)).refresh(mockPeripheral.gattConnection)

        verify(mockPeerGattServiceSubscriber, never())
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
        verify(mockGattServiceDiscoverer, times(1)).discover(mockGattConnection)

        testScheduler.advanceTimeBy(10, TimeUnit.SECONDS)
        verify(mockGattServiceDiscoverer, times(2)).discover(mockGattConnection)

        verify(mockPeerGattServiceSubscriber)
            .subscribe(mockPeripheral, GattlinkService.uuid, TransmitCharacteristic.uuid)

        testObserver
            .assertComplete()
            .dispose()
    }

    @Test
    fun shouldFailIfRemoteGattlinkServicAndFitbitGattlinkServiceNotFound() {
        mockDiscoverServicesSuccess()
        mockGattDatabaseValidatorSuccess()
        mockRemoteFitbitGattlinkServiceNotFound()
        whenever(mockPeripheral.gattConnection.getRemoteGattService(GattlinkService.uuid)).thenReturn(null)

        val testObserver = handler.link(mockPeripheral)
            .test()

        verify(mockGattServiceDiscoverer, times(1)).discover(mockGattConnection)
        verify(mockPeerGattServiceSubscriber)
            .subscribe(mockPeripheral, FitbitGattlinkService.uuid, TransmitCharacteristic.uuid)

        testObserver
            .assertFailure(GattServiceNotFoundException::class.java)
            .dispose()
    }

    private fun mockDiscoverServicesSuccess(services: List<BluetoothGattService> = emptyList()) =
        whenever(mockGattServiceDiscoverer.discover(mockGattConnection)).thenReturn(Single.just(services))

    private fun mockDiscoverServicesRetrySuccess(services: List<BluetoothGattService> = emptyList()) =
        whenever(mockGattServiceDiscoverer.discover(mockGattConnection)).thenReturn(
            Single.error(RuntimeException("Failed")),
            Single.just(services)
        )

    private fun mockDiscoverServicesFailure() =
        whenever(mockGattServiceDiscoverer.discover(mockGattConnection)).thenReturn(
            Single.error(RuntimeException("Failed")))

    private fun mockRemoteGattlinkSubscriptionSuccess() =
        whenever(mockPeerGattServiceSubscriber.subscribe(
            mockPeripheral,
            GattlinkService.uuid,
            TransmitCharacteristic.uuid
        )).thenReturn(Completable.complete())

    private fun mockRemoteFitbitGattlinkSubscriptionSuccess() =
        whenever(mockPeerGattServiceSubscriber.subscribe(
            mockPeripheral,
            FitbitGattlinkService.uuid,
            TransmitCharacteristic.uuid
        )).thenReturn(Completable.complete())

    private fun mockRemoteFitbitGattlinkServiceNotFound() =
        whenever(mockPeerGattServiceSubscriber.subscribe(
            mockPeripheral,
            FitbitGattlinkService.uuid,
            TransmitCharacteristic.uuid
        )).thenReturn(Completable.error(GattServiceNotFoundException(GattlinkService.uuid)))

    private fun mockGattDatabaseValidatorSuccess() =
        whenever(mockGattDatabaseValidator.validate(mockPeripheral)).thenReturn(Completable.complete())

    private fun mockGattDatabaseValidatorFailure() =
        whenever(mockGattDatabaseValidator.validate(mockPeripheral)).thenReturn(
            Completable.error(GattDatabaseValidatorException())
        )

}

package com.fitbit.goldengate.hub

import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattService
import com.fitbit.bluetooth.fbgatt.rx.GattCharacteristicSubscriptionStatus
import com.fitbit.bluetooth.fbgatt.rx.GattServiceNotFoundException
import com.fitbit.bluetooth.fbgatt.rx.client.BitGattPeer
import com.fitbit.bluetooth.fbgatt.rx.client.PeerGattServiceSubscriber
import com.fitbit.goldengate.bt.gatt.server.services.gattlink.listeners.TransmitCharacteristicSubscriptionListener
import com.fitbit.linkcontroller.services.configuration.ClientPreferredConnectionConfigurationCharacteristic
import com.fitbit.linkcontroller.services.configuration.ClientPreferredConnectionModeCharacteristic
import com.fitbit.linkcontroller.services.configuration.GeneralPurposeCommandCharacteristic
import com.fitbit.linkcontroller.services.configuration.LinkConfigurationService
import com.nhaarman.mockitokotlin2.doReturn
import com.nhaarman.mockitokotlin2.mock
import com.nhaarman.mockitokotlin2.times
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
import java.util.concurrent.TimeUnit
import java.util.concurrent.TimeoutException

class LinkupWithPeerHubHandlerTest {

    private val mockFitbitBluetoothDevice = com.fitbit.goldengate.bt.mockFitbitBluetoothDevice
    private val mockBluetoothGatt = mock<BluetoothGatt>()

    private val mockCentral = mock<BitGattPeer> {
        on { fitbitDevice } doReturn mockFitbitBluetoothDevice
        on { connect() } doReturn Single.just(mockBluetoothGatt)
    }

    private val mockPeerGattServiceSubscriber = mock<PeerGattServiceSubscriber>()
    private val mockTransmitCharacteristicSubscriptionListener = mock< TransmitCharacteristicSubscriptionListener>()
    private val linkUpTimeoutSeconds = 60L
    private val testScheduler = TestScheduler()

    private val handler = LinkupWithPeerHubHandler(
        mockPeerGattServiceSubscriber,
        mockTransmitCharacteristicSubscriptionListener,
        linkUpTimeoutSeconds,
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
    fun `complete the linkup procedure`() {
        mockDiscoverServicesSuccess()
        mockClientPreferredConnectionConfigurationCharacteristicSubscriptionSuccess()
        mockClientPreferredConnectionModeCharacteristicSubscriptionSuccess()
        mockGeneralPurposeCommandCharacteristicSubscriptionSuccess()
        mockWaitForGattlinkSubscriptionSuccess()

        val testObserver = handler.link(mockCentral).test()

        verify(mockCentral).discoverServices()
        verify(mockPeerGattServiceSubscriber)
            .subscribe(mockCentral, LinkConfigurationService.uuid, ClientPreferredConnectionConfigurationCharacteristic.uuid)
        verify(mockPeerGattServiceSubscriber)
            .subscribe(mockCentral, LinkConfigurationService.uuid, ClientPreferredConnectionModeCharacteristic.uuid)
        verify(mockPeerGattServiceSubscriber)
            .subscribe(mockCentral, LinkConfigurationService.uuid, GeneralPurposeCommandCharacteristic.uuid)

        testObserver
            .assertComplete()
            .dispose()
    }

    @Test
    fun `should fail if service discovery failed`() {
        mockDiscoverServicesFailure()

        val testObserver = handler.link(mockCentral).test()

        verify(mockCentral).discoverServices()
        verify(mockPeerGattServiceSubscriber, times(0))
            .subscribe(mockCentral, LinkConfigurationService.uuid, ClientPreferredConnectionConfigurationCharacteristic.uuid)

        testObserver
            .assertFailureAndMessage(RuntimeException::class.java, "Failed")
            .dispose()
    }

    @Test
    fun `should fail if Link configuration service is not found`() {
        mockDiscoverServicesSuccess()
        mockLinkConfigurationServiceNotFound()

        val testObserver = handler.link(mockCentral).test()

        verify(mockCentral).discoverServices()
        verify(mockPeerGattServiceSubscriber)
            .subscribe(mockCentral, LinkConfigurationService.uuid, ClientPreferredConnectionConfigurationCharacteristic.uuid)

        testObserver
            .assertFailure(GattServiceNotFoundException::class.java)
            .dispose()
    }

    @Test
    fun `fail to wait for gattlink subscription before timeout`() {
        mockDiscoverServicesSuccess()
        mockClientPreferredConnectionConfigurationCharacteristicSubscriptionSuccess()
        mockClientPreferredConnectionModeCharacteristicSubscriptionSuccess()
        mockGeneralPurposeCommandCharacteristicSubscriptionSuccess()
        mockWaitForGattlinkSubscriptionTooLate()

        val testObserver = handler.link(mockCentral).test()

        testScheduler.advanceTimeBy(LINK_UP_TIMEOUT_SECONDS, TimeUnit.SECONDS)

        verify(mockCentral).discoverServices()
        verify(mockPeerGattServiceSubscriber)
            .subscribe(mockCentral, LinkConfigurationService.uuid, ClientPreferredConnectionConfigurationCharacteristic.uuid)
        verify(mockPeerGattServiceSubscriber)
            .subscribe(mockCentral, LinkConfigurationService.uuid, ClientPreferredConnectionModeCharacteristic.uuid)
        verify(mockPeerGattServiceSubscriber)
            .subscribe(mockCentral, LinkConfigurationService.uuid, GeneralPurposeCommandCharacteristic.uuid)

        testObserver
            .assertFailure(TimeoutException::class.java)
            .dispose()
    }

    private fun mockDiscoverServicesSuccess(services: List<BluetoothGattService> = emptyList()) =
        whenever(mockCentral.discoverServices()).thenReturn(Single.just(services))

    private fun mockDiscoverServicesFailure() =
        whenever(mockCentral.discoverServices()).thenReturn(Single.error(RuntimeException("Failed")))

    private fun mockLinkConfigurationServiceNotFound() =
        whenever(mockPeerGattServiceSubscriber.subscribe(
            mockCentral,
            LinkConfigurationService.uuid,
            ClientPreferredConnectionConfigurationCharacteristic.uuid
        )).thenReturn(Completable.error(GattServiceNotFoundException(LinkConfigurationService.uuid)))

    private fun mockClientPreferredConnectionConfigurationCharacteristicSubscriptionSuccess() =
        whenever(mockPeerGattServiceSubscriber.subscribe(
            mockCentral,
            LinkConfigurationService.uuid,
            ClientPreferredConnectionConfigurationCharacteristic.uuid
        )).thenReturn(Completable.complete())

    private fun mockClientPreferredConnectionModeCharacteristicSubscriptionSuccess() =
        whenever(mockPeerGattServiceSubscriber.subscribe(
            mockCentral,
            LinkConfigurationService.uuid,
            ClientPreferredConnectionModeCharacteristic.uuid
        )).thenReturn(Completable.complete())

    private fun mockGeneralPurposeCommandCharacteristicSubscriptionSuccess() =
        whenever(mockPeerGattServiceSubscriber.subscribe(
            mockCentral,
            LinkConfigurationService.uuid,
            GeneralPurposeCommandCharacteristic.uuid
        )).thenReturn(Completable.complete())

    private fun mockWaitForGattlinkSubscriptionSuccess() =
        whenever(mockTransmitCharacteristicSubscriptionListener.getDataObservable(
            mockCentral.fitbitDevice.btDevice
        )).thenReturn(Observable.just(GattCharacteristicSubscriptionStatus.ENABLED))

    private fun mockWaitForGattlinkSubscriptionTooLate() =
        whenever(mockTransmitCharacteristicSubscriptionListener.getDataObservable(
            mockCentral.fitbitDevice.btDevice
        )).thenReturn(Observable.just(GattCharacteristicSubscriptionStatus.ENABLED).delay(LINK_UP_TIMEOUT_SECONDS, TimeUnit.SECONDS))

}

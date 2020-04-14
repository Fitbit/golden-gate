package com.fitbit.goldengate.bt

import android.content.Context
import com.fitbit.bluetooth.fbgatt.FitbitGatt
import com.fitbit.bluetooth.fbgatt.exception.BluetoothNotEnabledException
import com.fitbit.goldengate.bt.gatt.GattServerListenerRegistrar
import com.fitbit.linkcontroller.LinkControllerProvider
import com.nhaarman.mockitokotlin2.*
import io.reactivex.Completable
import io.reactivex.android.plugins.RxAndroidPlugins
import io.reactivex.plugins.RxJavaPlugins
import io.reactivex.schedulers.Schedulers
import org.junit.After
import org.junit.Before
import org.junit.Test
import org.mockito.ArgumentCaptor


class GlobalBluetoothGattInitializerTest {

    private val mockContext = mock<Context>()
    private val mockGattServerListenerRegistrar = mock<GattServerListenerRegistrar>()
    private val mockLinkControllerProvider = mock<LinkControllerProvider>()
    private val callbackCaptor = ArgumentCaptor.forClass(FitbitGatt.FitbitGattCallback::class.java)

    private val initializer = GlobalBluetoothGattInitializer(
        fitbitGatt = mockFitbitGatt,
        gattServerListenerRegistrar = mockGattServerListenerRegistrar,
        linkControllerProvider = mockLinkControllerProvider
    )

    @Before
    fun setup() {
        RxJavaPlugins.setIoSchedulerHandler { _ -> Schedulers.trampoline() }
        RxAndroidPlugins.setInitMainThreadSchedulerHandler { _ -> Schedulers.trampoline() }
        doNothing().whenever(mockFitbitGatt).registerGattEventListener(callbackCaptor.capture())
        whenever(mockGattServerListenerRegistrar.registerGattServerListeners(any())).thenReturn(Completable.complete())
    }

    @After
    fun tearDown() {
        initializer.stop()
    }

    @Test
    fun shouldStartBitgattAndAddLinkConfigurationIfBitgattStartSuccessful() {
        mockinitLinkControllerModule()

        initializer.start(mockContext)
        callbackCaptor.value.onGattServerStarted(mock())
        verifyInitLinkControllerCalled()
    }

    @Test
    fun shouldStartBitgattAndNotAddLinkConfigurationIfBitgattStartFails() {
        mockinitLinkControllerModule()

        initializer.start(mockContext)
        callbackCaptor.value.onGattServerStartError(BluetoothNotEnabledException())
        verifyInitLinkControllerNotCalled()
    }

    private fun mockinitLinkControllerModule() {
        whenever(mockLinkControllerProvider.addLinkConfigurationService()).thenReturn(Completable.complete())
    }

    private fun verifyInitLinkControllerCalled() = verifyInitLinkController(1)
    private fun verifyInitLinkControllerNotCalled() = verifyInitLinkController(0)
    private fun verifyInitLinkController(times: Int){
        verify(mockLinkControllerProvider, times(times)).addLinkConfigurationService()
    }
}

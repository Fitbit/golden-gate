// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bt

import android.content.Context
import com.fitbit.bluetooth.fbgatt.FitbitGatt
import com.fitbit.bluetooth.fbgatt.exception.BluetoothNotEnabledException
import com.fitbit.bluetooth.fbgatt.rx.server.BitGattServer
import com.fitbit.goldengate.bt.gatt.GattServerListenerRegistrar
import com.fitbit.goldengate.bt.gatt.server.services.gattcache.GattCacheService
import com.fitbit.goldengate.bt.gatt.server.services.gattcache.GattCacheServiceHandler
import com.fitbit.goldengate.bt.gatt.server.services.gattlink.GattlinkService
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
    private val mockGattServer = mock<BitGattServer>()
    private val mockGattCacheServiceHandler = mock<GattCacheServiceHandler>()
    private val mockGattlinkService = mock<GattlinkService>()

    private val initializer = GlobalBluetoothGattInitializer(
        fitbitGatt = mockFitbitGatt,
        gattServer = mockGattServer,
        gattServerListenerRegistrar = mockGattServerListenerRegistrar,
        linkControllerProvider = mockLinkControllerProvider,
        gattCacheServiceHandler = mockGattCacheServiceHandler,
        gattlinkServiceProvider = { mockGattlinkService }
    )

    @Before
    fun setup() {
        RxJavaPlugins.setIoSchedulerHandler { _ -> Schedulers.trampoline() }
        RxAndroidPlugins.setInitMainThreadSchedulerHandler { _ -> Schedulers.trampoline() }
        doNothing().whenever(mockFitbitGatt).registerGattEventListener(callbackCaptor.capture())
        whenever(mockGattServerListenerRegistrar.registerGattServerListeners(any())).thenReturn(Completable.complete())
        whenever(mockGattServerListenerRegistrar.registerGattServerNodeListeners(any())).thenReturn(Completable.complete())
        whenever(mockGattServer.addServices(any())).thenReturn(Completable.complete())
        whenever(mockGattCacheServiceHandler.addGattCacheService()).thenReturn(Completable.complete())
    }

    @After
    fun tearDown() {
        initializer.stop()
    }

    @Test
    fun `should start bitgatt as ble central and add link configuration if bitgatt start succeeds`() {
        mockinitLinkControllerModule()

        initializer.start(mockContext, true)
        callbackCaptor.value.onGattServerStarted(mock())
        verifyInitLinkControllerCalled()
    }

    @Test
    fun `should start bitgatt as central and not add link Configuration if bitgatt start fails`() {
        mockinitLinkControllerModule()

        initializer.start(mockContext, true)
        callbackCaptor.value.onGattServerStartError(BluetoothNotEnabledException())
        verifyInitLinkControllerNotCalled()
    }

    @Test
    fun `should start bitgatt as peripheral and then add gattlink and gatt cache services`() {
        initializer.start(mockContext, false)
        callbackCaptor.value.onGattServerStarted(mock())
        verifyInitGattlinkCalled()
        verifyInitGattCacheServiceCalled()
    }

    @Test
    fun `should start bitgatt as peripheral and not add gattlink and gatt cache services if bitgatt start fails`() {
        initializer.start(mockContext, false)
        callbackCaptor.value.onGattServerStartError(BluetoothNotEnabledException())
        verifyInitGattlinkNotCalled()
        verifyInitGattCacheServiceNotCalled()
    }

    private fun mockinitLinkControllerModule() {
        whenever(mockLinkControllerProvider.addLinkConfigurationService()).thenReturn(Completable.complete())
    }

    private fun verifyInitLinkControllerCalled() = verifyInitLinkController(1)
    private fun verifyInitLinkControllerNotCalled() = verifyInitLinkController(0)
    private fun verifyInitLinkController(times: Int){
        verify(mockLinkControllerProvider, times(times)).addLinkConfigurationService()
    }
    private fun verifyInitGattlinkCalled() = verifyInitGattlink(1)
    private fun verifyInitGattlinkNotCalled() = verifyInitGattlink(0)
    private fun verifyInitGattlink(times: Int){
        verify(mockGattServer, times(times)).addServices(mockGattlinkService)
    }
    private fun verifyInitGattCacheServiceCalled() = verifyInitGattCacheService(1)
    private fun verifyInitGattCacheServiceNotCalled() = verifyInitGattCacheService(0)
    private fun verifyInitGattCacheService(times: Int){
        verify(mockGattCacheServiceHandler, times(times)).addGattCacheService()
    }
}

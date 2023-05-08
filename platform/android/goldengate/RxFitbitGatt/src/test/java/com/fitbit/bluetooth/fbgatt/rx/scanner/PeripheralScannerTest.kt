// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.bluetooth.fbgatt.rx.scanner

import android.content.Context
import com.fitbit.bluetooth.fbgatt.FitbitGatt
import io.reactivex.plugins.RxJavaPlugins
import io.reactivex.schedulers.Schedulers
import org.junit.After
import org.junit.Before
import org.junit.Test
import org.mockito.kotlin.any
import org.mockito.kotlin.doReturn
import org.mockito.kotlin.mock
import org.mockito.kotlin.never
import org.mockito.kotlin.verify
import org.mockito.kotlin.whenever

class PeripheralScannerTest {

  private val context = mock<Context>()

  private val fitbitGatt = mock<FitbitGatt> { on { isScanning } doReturn true }
  @Before
  fun before() {
    RxJavaPlugins.setComputationSchedulerHandler { _ -> Schedulers.trampoline() }
  }

  @After
  fun tearDown() {
    RxJavaPlugins.reset()
  }

  @Test
  fun scanForDevices() {
    mockStartScanSuccess()

    PeripheralScanner(fitbitGatt = fitbitGatt)
      .scanForDevices(context, emptyList(), true)
      .test()
      .assertNotComplete()
      .dispose()

    verify(fitbitGatt).registerGattEventListener(any())
    verify(fitbitGatt).startHighPriorityScan(context)
    verify(fitbitGatt).unregisterGattEventListener(any())
    verify(fitbitGatt).resetScanFilters()
    verify(fitbitGatt).isScanning
    verify(fitbitGatt).cancelScan(context)
  }

  @Test
  fun shouldNotCancelScanOnDispose() {
    mockStartScanSuccess()

    PeripheralScanner(fitbitGatt = fitbitGatt)
      .scanForDevices(
        context,
        emptyList(),
        resetExistingFilters = true,
        cancelScanOnDispose = false
      )
      .test()
      .assertNotComplete()
      .dispose()

    verify(fitbitGatt, never()).cancelScan(context)
  }

  @Test
  fun shouldCompleteIfFailedToStartScan() {
    mockStartScanFailure()

    PeripheralScanner(fitbitGatt = fitbitGatt)
      .scanForDevices(context, emptyList())
      .test()
      .assertComplete()
  }

  private fun mockStartScanSuccess() =
    whenever(fitbitGatt.startHighPriorityScan(context)).thenReturn(true)

  private fun mockStartScanFailure() =
    whenever(fitbitGatt.startHighPriorityScan(context)).thenReturn(false)
}

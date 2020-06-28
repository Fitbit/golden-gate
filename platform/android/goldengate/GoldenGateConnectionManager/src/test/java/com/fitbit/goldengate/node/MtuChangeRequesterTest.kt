// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.node

import com.fitbit.bluetooth.fbgatt.rx.client.BitGattPeer
import com.fitbit.goldengate.bindings.stack.Stack
import com.fitbit.goldengate.bt.mockBluetoothAddress
import com.nhaarman.mockitokotlin2.mock
import com.nhaarman.mockitokotlin2.never
import com.nhaarman.mockitokotlin2.verify
import com.nhaarman.mockitokotlin2.whenever
import io.reactivex.Single
import io.reactivex.schedulers.TestScheduler
import org.junit.Test
import java.util.concurrent.TimeUnit
import java.util.concurrent.TimeoutException

class MtuChangeRequesterTest {

    private val requestedMtu = 10
    private val timeoutSeconds = 60L

    private val mockStack= mock<Stack>()
    private val mockPeripheral = mock<BitGattPeer>()

    private val testScheduler = TestScheduler()

    private val mtuChangeRequester = MtuChangeRequester(
        mockBluetoothAddress,
        mockStack,
        timeoutSeconds,
        testScheduler
    )

    @Test
    fun shouldUpdatePeripheralAndStackMtu() {
        mockUpdatePeripheralMtuSuccess()
        mockUpdateStackMtuSuccess()

        mtuChangeRequester.requestMtu(mockPeripheral, requestedMtu)
            .test()
            .assertValue { it == requestedMtu }

        verify(mockPeripheral).requestMtu(requestedMtu)
        verify(mockStack).updateMtu(requestedMtu)
    }

    @Test
    fun shouldTimeout() {
        mockUpdatePeripheralMtuNever()

        mtuChangeRequester.requestMtu(mockPeripheral, requestedMtu)
            .test()
            .also { testScheduler.advanceTimeBy(timeoutSeconds, TimeUnit.SECONDS) }
            .assertError(TimeoutException::class.java)

        verify(mockPeripheral).requestMtu(requestedMtu)
    }

    @Test
    fun shouldFailIfPeripheralUpdateFails() {
        mockUpdatePeripheralMtuFailure()
        mockUpdateStackMtuSuccess()

        mtuChangeRequester.requestMtu(mockPeripheral, requestedMtu)
                .test()
                .assertError { it is Exception }

        verify(mockPeripheral).requestMtu(requestedMtu)
        verify(mockStack, never()).updateMtu(requestedMtu)
    }

    @Test
    fun shouldFailIfStackUpdateFails() {
        mockUpdatePeripheralMtuSuccess()
        mockUpdateStackMtuFailure()

        mtuChangeRequester.requestMtu(mockPeripheral, requestedMtu)
                .test()
                .assertError { it is MtuChangeException }

        verify(mockPeripheral).requestMtu(requestedMtu)
        verify(mockStack).updateMtu(requestedMtu)
    }

    @Test
    fun shouldUpdateStackMtuWithActualPeripheralMtu() {
        val actualPeripheralMtu = 5
        mockUpdatePeripheralMtuSuccess(actualMtu = actualPeripheralMtu)
        mockUpdateStackMtuSuccess(actualPeripheralMtu)

        mtuChangeRequester.requestMtu(mockPeripheral, requestedMtu)
                .test()
                .assertValue { it == actualPeripheralMtu }

        verify(mockPeripheral).requestMtu(requestedMtu)
        verify(mockStack).updateMtu(actualPeripheralMtu)
    }

    @Test
    fun shouldResetStackMtuToDefaultIfPeripheralMtuUpdateFails() {
        mockUpdatePeripheralMtuFailure()
        mockUpdateStackMtuSuccess(DEFAULT_MIN_MTU)

        mtuChangeRequester.requestMtu(mockPeripheral, requestedMtu)
                .test()
                .assertValue { it == DEFAULT_MIN_MTU }

        verify(mockPeripheral).requestMtu(requestedMtu)
        verify(mockStack).updateMtu(DEFAULT_MIN_MTU)
    }

    private fun mockUpdatePeripheralMtuSuccess(requestedMtu: Int = this.requestedMtu, actualMtu: Int = this.requestedMtu) =
        whenever(mockPeripheral.requestMtu(requestedMtu)).thenReturn(Single.just(actualMtu))

    private fun mockUpdatePeripheralMtuNever(requestedMtu: Int = this.requestedMtu) =
        whenever(mockPeripheral.requestMtu(requestedMtu)).thenReturn(Single.never())

    private fun mockUpdatePeripheralMtuFailure(requestedMtu: Int = this.requestedMtu) =
            whenever(mockPeripheral.requestMtu(requestedMtu)).thenReturn(Single.error(Exception("Failed")))

    private fun mockUpdateStackMtuSuccess(requestedMtu: Int = this.requestedMtu) =
            whenever(mockStack.updateMtu(requestedMtu)).thenReturn(true)

    private fun mockUpdateStackMtuFailure(requestedMtu: Int = this.requestedMtu) =
            whenever(mockStack.updateMtu(requestedMtu)).thenReturn(false)

}

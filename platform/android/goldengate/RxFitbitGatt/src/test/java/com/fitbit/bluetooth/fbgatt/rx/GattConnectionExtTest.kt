// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.bluetooth.fbgatt.rx

import com.fitbit.bluetooth.fbgatt.GattState
import com.fitbit.bluetooth.fbgatt.TransactionResult
import com.fitbit.bluetooth.fbgatt.rx.client.GattClientConnectionStateSetter
import org.mockito.kotlin.any
import org.mockito.kotlin.doReturn
import org.mockito.kotlin.mock
import org.mockito.kotlin.never
import org.mockito.kotlin.verify
import org.mockito.kotlin.whenever
import io.reactivex.Completable
import org.junit.Test

class GattConnectionExtTest {

    private val mockTransactionResult: TransactionResult = mock()
    private val mockGattClientConnectionStateSetter: GattClientConnectionStateSetter = mock {
        on { set(any(), any()) } doReturn Completable.complete()
    }

    @Test
    fun shouldResetGattStateIfGattTransactionExceptionAndStateNotIdle() {
        mockGattStateNotIdle()

        mockGattConnection.resetGattStateIfNecessary(GattTransactionException(mockTransactionResult), mockGattClientConnectionStateSetter)
            .test()
            .assertComplete()

        verify(mockGattClientConnectionStateSetter).set(mockGattConnection, GattState.IDLE)
    }

    @Test
    fun shouldNotResetGattStateIfNotGattTransactionException() {
        mockGattStateNotIdle()

        mockGattConnection.resetGattStateIfNecessary(Exception(""), mockGattClientConnectionStateSetter)
            .test()
            .assertComplete()

        verify(mockGattClientConnectionStateSetter, never()).set(any(), any())
    }

    @Test
    fun shouldNotResetGattStateIfGattTransactionExceptionAndStateIdle() {
        mockGattStateIdle()

        mockGattConnection.resetGattStateIfNecessary(GattTransactionException(mockTransactionResult), mockGattClientConnectionStateSetter)
            .test()
            .assertComplete()

        verify(mockGattClientConnectionStateSetter, never()).set(any(), any())
    }

    private fun mockGattStateIdle() = mockGattState(GattState.IDLE)

    private fun mockGattStateNotIdle() = mockGattState(GattState.FAILURE_CONNECTING)

    private fun mockGattState(state: GattState) = whenever(mockGattConnection.gattState).thenReturn(state)
}

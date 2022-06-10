// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bindings.coap

import com.fitbit.goldengate.bindings.coap.data.Data
import com.fitbit.goldengate.bindings.coap.data.IncomingResponse
import com.fitbit.goldengate.bindings.coap.data.Options
import com.fitbit.goldengate.bindings.coap.data.OutgoingRequest
import com.fitbit.goldengate.bindings.coap.data.RawResponseMessage
import com.fitbit.goldengate.bindings.coap.data.ResponseCode
import com.nhaarman.mockitokotlin2.any
import com.nhaarman.mockitokotlin2.doReturn
import com.nhaarman.mockitokotlin2.mock
import com.nhaarman.mockitokotlin2.times
import com.nhaarman.mockitokotlin2.verify
import com.nhaarman.mockitokotlin2.verifyNoMoreInteractions
import io.reactivex.Observer
import io.reactivex.SingleEmitter
import org.junit.Test
import java.lang.ref.WeakReference

class SingleCoapResponseListenerTest {

    private val mockProgressObserver = mock<Observer<Int>>()
    private val mockRequest = mock<OutgoingRequest> {
        on { progressObserver } doReturn mockProgressObserver
    }
    private val mockSingleEmitter = mock<SingleEmitter<IncomingResponse>>()

    private val listener = SingleCoapResponseListener(
        mockRequest,
        mockSingleEmitter,
        WeakReference(null)
    )

    @Test
    fun shouldNotSignalForAckCallback() {
        listener.onAck()

        verifyNoMoreInteractions(mockSingleEmitter)
    }

    @Test
    fun shouldSignalErrorForErrorCallback() {
        listener.onError(1, "error message")

        verify(mockSingleEmitter).onError(any<CoapEndpointException>())
        verify(mockProgressObserver).onError(any<CoapEndpointException>())
    }

    @Test
    fun shouldSignalSuccessForNextCallback() {
        val code = ResponseCode.created
        val options = Options()
        val data = Data(0)
        val testMessage = RawResponseMessage(code, options, data)

        listener.onNext(testMessage)

        verify(mockSingleEmitter).onSuccess(any())
        verify(mockProgressObserver).onComplete()
    }

    @Test
    fun shouldNotSignalForCompleteCallback() {
        listener.onComplete()
        verify(mockProgressObserver, times(0)).onComplete()
        verifyNoMoreInteractions(mockSingleEmitter)
    }
}

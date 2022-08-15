// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bindings.coap.block

import com.fitbit.goldengate.bindings.coap.CoapEndpointException
import com.fitbit.goldengate.bindings.coap.CoapEndpointResponseException
import com.fitbit.goldengate.bindings.coap.data.ExtendedError
import com.fitbit.goldengate.bindings.coap.data.ExtendedErrorDecoder
import com.fitbit.goldengate.bindings.coap.data.IncomingResponse
import com.fitbit.goldengate.bindings.coap.data.Options
import com.fitbit.goldengate.bindings.coap.data.OutgoingRequest
import com.fitbit.goldengate.bindings.coap.data.RawResponseMessage
import com.fitbit.goldengate.bindings.coap.data.ResponseCode
import com.nhaarman.mockitokotlin2.any
import com.nhaarman.mockitokotlin2.argumentCaptor
import com.nhaarman.mockitokotlin2.doReturn
import com.nhaarman.mockitokotlin2.mock
import com.nhaarman.mockitokotlin2.times
import com.nhaarman.mockitokotlin2.verify
import com.nhaarman.mockitokotlin2.verifyNoMoreInteractions
import com.nhaarman.mockitokotlin2.whenever
import io.reactivex.Observer
import io.reactivex.SingleEmitter
import org.junit.Assert.assertEquals
import org.junit.Test
import java.util.Arrays
import kotlin.test.assertTrue

class BlockwiseCoapResponseListenerTest {
    private val mockProgressObserver = mock<Observer<Int>>()
    private val mockRequest = mock<OutgoingRequest> {
        on { progressObserver } doReturn mockProgressObserver
        on { expectSuccess } doReturn  true
    }
    private val mockSingleEmitter = mock<SingleEmitter<IncomingResponse>>()
    private val mockDecoder = mock<ExtendedErrorDecoder>()
    private val mockCancellable = mock<() -> Unit>()

    private val listener = BlockwiseCoapResponseListener(
            mockRequest,
            mockSingleEmitter,
            mockDecoder
    ).also { it.setCancellable(mockCancellable) }

    private val testCode = ResponseCode.created
    private val testData = "Hello,".toByteArray()
    private val testOptions = Options()
    private val testMessage = RawResponseMessage(testCode, testOptions, testData)

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
    fun shouldOnlySignalResponseListenerAndNotBodyOnFirstOnNextCallback() {

        listener.onNext(testMessage)

        val response = verifySuccessSignalOnResponseEmitter()
        assertEquals(testCode, response.responseCode)

        verify(mockSingleEmitter).onSuccess(any())
        response.body.asData().test()
                .assertNotComplete()
        verify(mockProgressObserver,times(0)).onComplete()
    }

    @Test
    fun shouldSignalResponseListenerAndBodyOnNextAndCompleteCallback() {

        listener.onNext(testMessage)
        listener.onComplete()

        val response = verifySuccessSignalOnResponseEmitter()
        assertEquals(testCode, response.responseCode)

        verify(mockSingleEmitter).onSuccess(any())
        response.body.asData().test()
                .assertValue { Arrays.equals(testData, it) }
        verify(mockProgressObserver).onComplete()
    }

    @Test
    fun shouldNotSignalResponseListenerOnOnlyOnCompleteCallback() {

        listener.onComplete()

        verifyNoMoreInteractions(mockSingleEmitter)
    }

    @Test
    fun shouldSignalBodyWithAggregateBlockDataFromOnNextCallback() {
        val testData2 = " World".toByteArray()
        val testMessage2 = RawResponseMessage(testCode, testOptions, testData2)
        val expectedBody = "Hello, World".toByteArray()

        listener.onNext(testMessage)
        listener.onNext(testMessage2)
        listener.onComplete()

        val response = verifySuccessSignalOnResponseEmitter()

        response.body.asData().test()
                .assertValue { Arrays.equals(expectedBody, it) }
        verify(mockProgressObserver).onComplete()
    }

    @Test
    fun shouldSendPartialCachedDataIfFailureBeforeReceivingAllBlocks() {
        // this is the partial data received byt the successful [onNext] calls
        val expectedPartialData = testData

        listener.onNext(testMessage)
        listener.onError(1, "error message")

        val response = verifySuccessSignalOnResponseEmitter()

        response.body.asData().test()
                .assertError {
                    it is CoapEndpointException && Arrays.equals(expectedPartialData, it.partialData)
                }
    }

    @Test
    fun shouldSendErrorIfFailureReceivingFirstBlocks() {
        val expectedEmptyPartialData = ByteArray(0)

        // first callback is error without any data received
        listener.onError(1, "error message")

        val response = verifyFailureSignalOnResponseEmitter()

        // exception has empty data as first block failed
        assertTrue(Arrays.equals(expectedEmptyPartialData, response.partialData))
    }

    @Test
    fun shouldSignalBodyWithErrorIfReceivedErrorMessage() {
        val testData2 = " World".toByteArray()
        val testMessage2 = RawResponseMessage(ResponseCode.internalServerError, testOptions, testData2)

        val mockExtendedError = mock<ExtendedError>()
        whenever( mockDecoder.decode(testMessage2)).doReturn(mockExtendedError)

        listener.onNext(testMessage)
        listener.onNext(testMessage2)

        val response = verifySuccessSignalOnResponseEmitter()

        response.body.asData().test()
            .assertError {
            it is CoapEndpointResponseException && it.responseCode == ResponseCode.internalServerError && it.extendedError == mockExtendedError
        }
        verify(mockProgressObserver).onError(any())
    }

    @Test
    fun shouldCallCancellableWhenBodyStreamIsCancelledAfterReceivingFirstBlock() {
        listener.onNext(testMessage)

        val response = verifySuccessSignalOnResponseEmitter()

        // cancel the body stream; set cancelled = true
        response.body.asData().test(true)

        verify(mockCancellable).invoke()
    }

    @Test
    fun shouldCallCancellableWhenBodyStreamIsCancelledAfterReceivingTwoBlocks() {
        val testData2 = " World".toByteArray()
        val testMessage2 = RawResponseMessage(testCode, testOptions, testData2)

        listener.onNext(testMessage)
        listener.onNext(testMessage2)

        val response = verifySuccessSignalOnResponseEmitter()

        // cancel the body stream; set cancelled = true
        response.body.asData().test(true)

        verify(mockCancellable).invoke()
    }

    private fun verifySuccessSignalOnResponseEmitter(): IncomingResponse {
        val responseCaptor = argumentCaptor<IncomingResponse>()
        verify(mockSingleEmitter).onSuccess(responseCaptor.capture())
        return responseCaptor.firstValue
    }

    private fun verifyFailureSignalOnResponseEmitter(): CoapEndpointException {
        val responseCaptor = argumentCaptor<CoapEndpointException>()
        verify(mockSingleEmitter).onError(responseCaptor.capture())
        return responseCaptor.firstValue
    }
}

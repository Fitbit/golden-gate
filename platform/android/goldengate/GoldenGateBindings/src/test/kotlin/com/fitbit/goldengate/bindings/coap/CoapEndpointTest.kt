// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bindings.coap

import com.fitbit.goldengate.bindings.BaseTest
import com.fitbit.goldengate.bindings.coap.data.Block1Option
import com.fitbit.goldengate.bindings.coap.data.ContentFormatOption
import com.fitbit.goldengate.bindings.coap.data.EtagOption
import com.fitbit.goldengate.bindings.coap.data.FormatOptionValue
import com.fitbit.goldengate.bindings.coap.data.IfNoneMatchOption
import com.fitbit.goldengate.bindings.coap.data.MaxAgeOption
import com.fitbit.goldengate.bindings.coap.data.Method
import com.fitbit.goldengate.bindings.coap.data.OutgoingRequestBuilder
import com.fitbit.goldengate.bindings.coap.data.ResponseCode
import com.fitbit.goldengate.bindings.coap.data.UriPathOption
import com.fitbit.goldengate.bindings.coap.data.UriQueryOption
import com.fitbit.goldengate.bindings.coap.handler.EchoResourceHandler
import com.fitbit.goldengate.bindings.io.RxSource
import com.fitbit.goldengate.bindings.io.TxSink
import com.fitbit.goldengate.bindings.node.BluetoothAddressNodeKey
import com.fitbit.goldengate.bindings.stack.SocketNetifGattlink
import com.fitbit.goldengate.bindings.stack.Stack
import org.mockito.kotlin.*
import io.reactivex.Observer
import io.reactivex.schedulers.Schedulers
import org.junit.After
import org.junit.Assert.*
import org.junit.Test
import java.util.concurrent.CountDownLatch
import java.util.concurrent.Executors
import java.util.concurrent.TimeUnit

class CoapEndpointTest : BaseTest() {

    private var endpoint1: CoapEndpoint? = null
    private var endpoint2: CoapEndpoint? = null

    @After
    fun tearDown() {
        endpoint1?.close()
        endpoint2?.close()

        endpoint1 = null
        endpoint2 = null
    }

    @Test
    fun canCreate() {
        endpoint1 = CoapEndpoint()
        assertTrue(endpoint1!!.thisPointerWrapper > 0)
    }

    @Test
    fun canAttachToDifferentEndpoint() {
        endpoint1 = CoapEndpoint()
        endpoint2 = CoapEndpoint()
        endpoint1!!.attach(endpoint2!!)
    }

    @Test
    fun shouldSuccessfullySendRequest() {
        shouldSuccessfullySendRequest(false)
        shouldSuccessfullySendRequest(true)
        shouldSuccessfullySendRequestWithProgressObserver()
        shouldSuccessfullySendRequestWithZeroMaxResendCount()
    }

    private fun shouldSuccessfullySendRequest(forceNonBlockwise: Boolean) {
        endpoint1 = CoapEndpoint()
        endpoint2 = CoapEndpoint()
        endpoint1!!.attach(endpoint2!!)

        // building max/full request object fully test jni bindings
        val request = OutgoingRequestBuilder("echo", Method.POST)
            .forceNonBlockwise(forceNonBlockwise)
            .option(ContentFormatOption(FormatOptionValue.TEXT_PLAIN))
            .option(MaxAgeOption(1))
            .option(UriQueryOption("query_1=value_1"))
            .option(EtagOption("etag".toByteArray()))
            .option(IfNoneMatchOption)
            .body("hello".toByteArray())
            .build()

        /**
         * Intentionally non-blocking call as purpose here is to verify jni binding
         * and creation of request. This test will help us catch any error reading kotlin
         * objects/data from jni code
         */
        endpoint1!!.responseFor(request).test()
    }

    @Test
    fun shouldRespondOnGivenScheduler() {
        val latch = CountDownLatch(1)
        val scheduler = Schedulers.from(Executors.newSingleThreadExecutor {
            Thread {
                latch.countDown()
                it.run()
            }
        })

        endpoint1 = CoapEndpoint(scheduler)
        endpoint2 = CoapEndpoint()
        endpoint1!!.attach(endpoint2!!)

        // building max/full request object fully test jni bindings
        val request = OutgoingRequestBuilder("echo", Method.POST)
            .forceNonBlockwise(true)
            .option(ContentFormatOption(FormatOptionValue.TEXT_PLAIN))
            .option(MaxAgeOption(1))
            .option(UriQueryOption("query_1=value_1"))
            .option(EtagOption("etag".toByteArray()))
            .option(IfNoneMatchOption)
            .body("hello".toByteArray())
            .build()

        endpoint1!!.responseFor(request).subscribe()
        assertTrue(latch.await(50, TimeUnit.MILLISECONDS))
    }

    private fun shouldSuccessfullySendRequestWithProgressObserver() {
        endpoint1 = CoapEndpoint()
        endpoint2 = CoapEndpoint()
        endpoint1!!.attach(endpoint2!!)
        val observer = mock<Observer<Int>>()

        // building max/full request object fully test jni bindings
        val request = OutgoingRequestBuilder("echo", Method.POST)
            .progressObserver(observer)
            .option(ContentFormatOption(FormatOptionValue.TEXT_PLAIN))
            .option(MaxAgeOption(1))
            .option(UriQueryOption("query_1=value_1"))
            .option(EtagOption("etag".toByteArray()))
            .option(IfNoneMatchOption)
            .body("hello".toByteArray())
            .build()

        /**
         * Intentionally non-blocking call as purpose here is to verify jni binding
         * and creation of request. This test will help us catch any error reading kotlin
         * objects/data from jni code
         */
        endpoint1!!.responseFor(request).test()
    }

    private fun shouldSuccessfullySendRequestWithZeroMaxResendCount() {
        endpoint1 = CoapEndpoint()
        endpoint2 = CoapEndpoint()
        endpoint1!!.attach(endpoint2!!)
        val observer = mock<Observer<Int>>()

        // building max/full request object fully test jni bindings
        val request = OutgoingRequestBuilder("echo", Method.POST)
            .maxResendCount(0)
            .progressObserver(observer)
            .option(ContentFormatOption(FormatOptionValue.TEXT_PLAIN))
            .option(MaxAgeOption(1))
            .option(UriQueryOption("query_1=value_1"))
            .option(EtagOption("etag".toByteArray()))
            .option(IfNoneMatchOption)
            .body("hello".toByteArray())
            .build()

        /**
         * Intentionally non-blocking call as purpose here is to verify jni binding
         * and creation of request. This test will help us catch any error reading kotlin
         * objects/data from jni code
         */
        endpoint1!!.responseFor(request).test()
    }

    @Test
    fun shouldSuccessfullyBuildRequestWithNegativeMaxResendCount() {
        OutgoingRequestBuilder("echo", Method.POST)
            .maxResendCount(-1)
            .ackTimeout(1000)
            .option(ContentFormatOption(FormatOptionValue.TEXT_PLAIN))
            .option(MaxAgeOption(1))
            .option(UriQueryOption("query_1=value_1"))
            .option(EtagOption("etag".toByteArray()))
            .option(IfNoneMatchOption)
            .body("hello".toByteArray())
            .build()
    }

    @Test
    fun shouldSuccessfullyBuildRequestWithNegativeTimeout() {
        endpoint1 = CoapEndpoint()
        val request = OutgoingRequestBuilder("echo", Method.POST)
            .ackTimeout(-1000)
            .option(ContentFormatOption(FormatOptionValue.TEXT_PLAIN))
            .option(MaxAgeOption(1))
            .option(UriQueryOption("query_1=value_1"))
            .option(EtagOption("etag".toByteArray()))
            .option(IfNoneMatchOption)
            .body("hello".toByteArray())
            .build()

        endpoint1!!.responseFor(request).test()
    }

    @Test
    fun shouldReturnResponseIfServerSupportsRequest() {
        endpoint1 = CoapEndpoint()
        endpoint2 = CoapEndpoint()

        endpoint2!!.addResourceHandler(
            "echo", EchoResourceHandler(),
            CoapEndpointHandlerConfiguration(CoapGroupRequestFilterMode.GROUP_1)
        )
            .blockingAwait()

        endpoint1!!.attach(endpoint2!!)
        val observer = mock<Observer<Int>>()

        val bodyString = "testing body slightly larger than hello world string"
        val request = OutgoingRequestBuilder("echo", Method.POST)
            .progressObserver(observer)
            .option(IfNoneMatchOption)
            .option(MaxAgeOption(1))
            .option(EtagOption("etag_1".toByteArray()))
            .option(UriQueryOption("query_1=value_1"))
            .body(bodyString.toByteArray())
            .build()
        val response = endpoint1!!.responseFor(request).blockingGet()

        assertEquals(ResponseCode.created, response.responseCode)
        assertEquals(bodyString, String(response.body.asData().blockingGet()))

        // size == options + one for each subpath
        assertEquals(6, response.options.size)
        assertTrue(response.options.contains(UriPathOption("echo")))
        assertTrue(response.options.contains(IfNoneMatchOption))
        assertTrue(response.options.contains(MaxAgeOption(1)))
        assertTrue(response.options.contains(EtagOption("etag_1".toByteArray())))
        assertTrue(response.options.contains(UriQueryOption("query_1=value_1")))
        assertTrue(response.options.contains(Block1Option(0x6)))
        verify(observer).onNext(0)
        verify(observer).onComplete()
    }

    @Test
    fun shouldReturnResponseIfServerSupportsRequestAsInputStream() {
        endpoint1 = CoapEndpoint()
        endpoint2 = CoapEndpoint()

        endpoint2!!.addResourceHandler(
            "echo", EchoResourceHandler(),
            CoapEndpointHandlerConfiguration(CoapGroupRequestFilterMode.GROUP_1)
        )
            .blockingAwait()

        endpoint1!!.attach(endpoint2!!)
        val observer = mock<Observer<Int>>()

        val bodyString = "testing body slightly larger than hello world string"
        val request = OutgoingRequestBuilder("echo", Method.POST)
            .progressObserver(observer)
            .option(IfNoneMatchOption)
            .option(MaxAgeOption(1))
            .option(EtagOption("etag_1".toByteArray()))
            .option(UriQueryOption("query_1=value_1"))
            .body(bodyString.toByteArray().inputStream())
            .build()
        val response = endpoint1!!.responseFor(request).blockingGet()

        assertEquals(ResponseCode.created, response.responseCode)
        assertEquals(bodyString, String(response.body.asData().blockingGet()))

        // size == options + one for each subpath
        assertEquals(6, response.options.size)
        assertTrue(response.options.contains(UriPathOption("echo")))
        assertTrue(response.options.contains(IfNoneMatchOption))
        assertTrue(response.options.contains(MaxAgeOption(1)))
        assertTrue(response.options.contains(EtagOption("etag_1".toByteArray())))
        assertTrue(response.options.contains(UriQueryOption("query_1=value_1")))
        assertTrue(response.options.contains(Block1Option(0x6)))
        verify(observer).onNext(0)
        verify(observer).onComplete()
    }


    @Test
    fun shouldCallOnErrorWhenRegisteringSamePathTwice() {
        endpoint1 = CoapEndpoint()

        endpoint1!!.addResourceHandler("echo", EchoResourceHandler())
            .test()
            .assertNoErrors()
        endpoint1!!.addResourceHandler("echo", EchoResourceHandler())
            .test()
            .assertError(CoapEndpointException::class.java)
    }

    @Test
    fun shouldReturnErrorIfServerDoesNotHaveAnyHandlerForRequestedPath() {
        shouldReturnErrorIfServerDoesNotHaveAnyHandlerForRequestedPathExpectSuccess(false)
        shouldReturnErrorIfServerDoesNotHaveAnyHandlerForRequestedPathExpectSuccess(true)
        shouldReturnErrorIfServerDoesNotHaveAnyHandlerForRequestedPathNoExpectSuccess(false)
        shouldReturnErrorIfServerDoesNotHaveAnyHandlerForRequestedPathNoExpectSuccess(true)
    }

    private fun shouldReturnErrorIfServerDoesNotHaveAnyHandlerForRequestedPathExpectSuccess(
        forceNonBlockwise: Boolean
    ) {
        endpoint1 = CoapEndpoint()
        endpoint2 = CoapEndpoint()
        endpoint1!!.attach(endpoint2!!)

        val request = OutgoingRequestBuilder("non_existing_path", Method.GET)
            .expectSuccess(false)
            .forceNonBlockwise(forceNonBlockwise)
            .build()

        val response = endpoint1!!.responseFor(request).blockingGet()
        assertEquals(ResponseCode.notFound, response.responseCode)
    }

    private fun shouldReturnErrorIfServerDoesNotHaveAnyHandlerForRequestedPathNoExpectSuccess(
        forceNonBlockwise: Boolean
    ) {
        endpoint1 = CoapEndpoint()
        endpoint2 = CoapEndpoint()
        endpoint1!!.attach(endpoint2!!)

        val request = OutgoingRequestBuilder("non_existing_path", Method.GET)
            .expectSuccess(true)
            .forceNonBlockwise(forceNonBlockwise)
            .build()

        endpoint1!!.responseFor(request)
            .test()
            .assertError { t -> t is CoapEndpointResponseException && t.responseCode == ResponseCode.notFound }
    }

    @Test
    fun shouldReturnErrorIfServerDoesNotSupportRequestedMethod() {
        endpoint1 = CoapEndpoint()
        endpoint2 = CoapEndpoint()

        endpoint2!!.addResourceHandler(
            "echo", EchoResourceHandler(),
            CoapEndpointHandlerConfiguration(CoapGroupRequestFilterMode.GROUP_1)
        )
            .blockingAwait()

        endpoint1!!.attach(endpoint2!!)

        // echo only supports POST method
        val request = OutgoingRequestBuilder("echo", Method.GET)
            .expectSuccess(false)
            .forceNonBlockwise(true)
            .body("hello".toByteArray())
            .build()
        val response = endpoint1!!.responseFor(request).blockingGet()

        assertEquals(ResponseCode.methodNotAllowed, response.responseCode)
    }

    @Test
    fun shouldReturnErrorIfClientIsUnauthroized() {
        endpoint1 = CoapEndpoint()
        endpoint2 = CoapEndpoint()

        endpoint2!!.addResourceHandler("echo", EchoResourceHandler()).blockingAwait()

        endpoint1!!.attach(endpoint2!!)
        val observer = mock<Observer<Int>>()

        val bodyString = "testing body slightly larger than hello world string"
        val request = OutgoingRequestBuilder("echo", Method.POST)
            .progressObserver(observer)
            .option(IfNoneMatchOption)
            .option(MaxAgeOption(1))
            .option(EtagOption("etag_1".toByteArray()))
            .option(UriQueryOption("query_1=value_1"))
            .body(bodyString.toByteArray())
            .build()
        val response = endpoint1!!.responseFor(request).blockingGet()

        assertEquals(ResponseCode.unauthorized, response.responseCode)
    }

    @Test
    fun shouldReturnResponseIfServerSupportsRequestAndClientIsAuthorized() {
        endpoint1 = CoapEndpoint()
        endpoint2 = CoapEndpoint()

        endpoint2!!.requestFilter.setGroupTo(CoapGroupRequestFilterMode.GROUP_0)

        endpoint2!!.addResourceHandler("echo", EchoResourceHandler()).blockingAwait()

        endpoint1!!.attach(endpoint2!!)
        val observer = mock<Observer<Int>>()

        val bodyString = "testing body slightly larger than hello world string"
        val request = OutgoingRequestBuilder("echo", Method.POST)
            .progressObserver(observer)
            .option(IfNoneMatchOption)
            .option(MaxAgeOption(1))
            .option(EtagOption("etag_1".toByteArray()))
            .option(UriQueryOption("query_1=value_1"))
            .body(bodyString.toByteArray())
            .build()
        val response = endpoint1!!.responseFor(request).blockingGet()

        assertEquals(ResponseCode.created, response.responseCode)
        assertEquals(bodyString, String(response.body.asData().blockingGet()))

        // size == options + one for each subpath
        assertEquals(6, response.options.size)
        assertTrue(response.options.contains(UriPathOption("echo")))
        assertTrue(response.options.contains(IfNoneMatchOption))
        assertTrue(response.options.contains(MaxAgeOption(1)))
        assertTrue(response.options.contains(EtagOption("etag_1".toByteArray())))
        assertTrue(response.options.contains(UriQueryOption("query_1=value_1")))
        assertTrue(response.options.contains(Block1Option(0x6)))
        verify(observer).onNext(0)
        verify(observer).onComplete()
    }

    @Test
    fun canAddHandlerWithSamePathAfterPreviousOneWasDisposed() {
        endpoint1 = CoapEndpoint()

        endpoint1!!.addResourceHandler("echo", EchoResourceHandler())
            .test()
            .assertNoErrors()

        endpoint1!!.removeResourceHandler("echo").test()
        endpoint1!!.addResourceHandler("echo", EchoResourceHandler())
            .test()
            .assertNoErrors()
    }

    @Test
    fun canAddDifferentHandlerWithSamePath() {
        endpoint1 = CoapEndpoint()

        endpoint1!!.addResourceHandler("echo_1", EchoResourceHandler())
            .test()
            .assertNoErrors()
        endpoint1!!.addResourceHandler("echo_2", EchoResourceHandler())
            .test()
            .assertNoErrors()
    }

    @Test
    fun canAttachAndDetach() {
        endpoint1 = CoapEndpoint()

        var gattlinkPacketDetected = false
        var udpPacketDetected = false

        val txSink = TxSink()
        txSink.dataObservable.observeOn(Schedulers.newThread()).subscribe(
            {
                if (it.size > 5) { // This is a check to ensure that no UDP packets come out of the stack, just gattlink packets.
                    fail()
                } else {
                    gattlinkPacketDetected = true
                }
            },
            {
                udpPacketDetected = true
            }) // Record that there was a UDP packet emitted from the stack.
        val rxSource = RxSource()
        val stack = Stack(
            BluetoothAddressNodeKey("AA:BB:CC:DD:EE:FF"),
            txSink.thisPointer,
            rxSource.thisPointer,
            SocketNetifGattlink()
        )
        stack.start()
        rxSource.receiveData(byteArrayOf(-127, 0, 0, 8, 8))
            .blockingAwait() //trick gattlink into thinking it's talking to another stack

        endpoint1!!.attach(stack) //Attach the coap endpoint to the stack
        endpoint1!!.detach() //Detach the coap endpoint from the stack. Comment this line and verify that the test fails.

        val data = byteArrayOf(1, 2, 3) //just some data
        val outgoingRequest =
            OutgoingRequestBuilder("echo", Method.POST).maxResendCount(0).body(data).build()

        // Trigger the coap endpoint to try to send a packet into the stack (if it's detached it won't actually be able to)
        // onErrorReturn { mock() } is just to prevent blockingGet from throwing on the timeout. An alternative would be to wrap it in try/catch.
        endpoint1!!.responseFor(outgoingRequest)
            .timeout(50, TimeUnit.MILLISECONDS, Schedulers.computation()).onErrorReturn { mock() }
            .blockingGet()

        assertFalse(udpPacketDetected)
        assertTrue(gattlinkPacketDetected)
    }
}

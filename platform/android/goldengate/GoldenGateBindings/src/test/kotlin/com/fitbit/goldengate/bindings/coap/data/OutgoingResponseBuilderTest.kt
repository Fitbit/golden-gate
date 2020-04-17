// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bindings.coap.data

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import java.util.Arrays

class OutgoingResponseBuilderTest {

    @Test
    fun shouldBuildWithDefaultResponseCode() {
        val response = OutgoingResponseBuilder().build()

        assertEquals(ResponseCode.Class.ok, response.responseCode.responseClass)
        assertEquals(1.toByte(), response.responseCode.detail)
        assertEquals(ResponseCode.created, response.responseCode)
    }

    @Test
    fun shouldBuildWithGivenResponseCode() {
        val response = OutgoingResponseBuilder()
            .responseCode(ResponseCode.methodNotAllowed)
            .build()

        assertEquals(ResponseCode.Class.clientError, response.responseCode.responseClass)
        assertEquals(5.toByte(), response.responseCode.detail)
        assertEquals(ResponseCode.methodNotAllowed, response.responseCode)
    }

    @Test
    fun shouldBuildRequestWithBody() {
        val testByteArray = "testData".toByteArray()
        val request = OutgoingResponseBuilder().body(testByteArray).build()

        assertTrue(Arrays.equals(testByteArray, (request.body as BytesArrayOutgoingBody).data))
    }

    @Test
    fun shouldBuildRequestWithOptions() {
        val request = OutgoingResponseBuilder()
            .option(AcceptOption(FormatOptionValue.TEXT_PLAIN))
            .option(EtagOption("etag".toByteArray()))
            .build()

        assertNotNull(request.options.firstOrNull { it.number ==  OptionNumber.ACCEPT})
        assertNotNull(request.options.firstOrNull { it.number ==  OptionNumber.ETAG})
    }

}

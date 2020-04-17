// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bindings.coap.data

import com.fitbit.goldengate.bindings.BaseTest
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class ExtendedErrorDecoderTest : BaseTest() {

    private val decoder = ExtendedErrorDecoder()

    private val extendedError: Data = byteArrayOf(
        0x0a, 0x0f, 0x6f, 0x72, 0x67, 0x2e, 0x65, 0x78, 0x61, 0x6d, 0x70, 0x6c, 0x65, 0x2e, 0x66,
        0x6f, 0x6f, 0x10, 0xab.toByte(), 0x02, 0x1a, 0x05, 0x68, 0x65, 0x6c, 0x6c, 0x6f
    )

    @Test
    fun shouldDecodeExtendedError() {

        val extendedError = decoder.decode(
            RawResponseMessage(
                responseCode = ResponseCode.serviceUnavailable,
                options = Options(),
                data = extendedError
            )
        )

        assertEquals("org.example.foo", extendedError?.namespace)
        assertEquals(-150, extendedError?.code)
        assertEquals("hello", extendedError?.message)
    }

    @Test
    fun shouldDecodeExtendedErrorWithExtraFields() {
        val extendedErrorWithExtraUnknownFields: Data = byteArrayOf(
            0x0a, 0x0f, 0x6f, 0x72, 0x67, 0x2e, 0x65, 0x78, 0x61, 0x6d, 0x70, 0x6c, 0x65, 0x2e, 0x66,
            0x6f, 0x6f, 0x10, 0xab.toByte(), 0x02, 0x1a, 0x05, 0x68, 0x65, 0x6c, 0x6c, 0x6f, 0x25,
            0xd2.toByte(), 0x04, 0x00, 0x00, 0x29, 0x2e, 0x16, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x32,
            0x03, 0x62, 0x61, 0x72
        )

        val extendedError = decoder.decode(
            RawResponseMessage(
                responseCode = ResponseCode.serviceUnavailable,
                options = Options(),
                data = extendedErrorWithExtraUnknownFields
            )
        )

        assertEquals("org.example.foo", extendedError?.namespace)
        assertEquals(-150, extendedError?.code)
        assertEquals("hello", extendedError?.message)
    }

    @Test
    fun shouldDecodeExtendedErrorWithNoMessageField() {
        val extendedErrorWithNullMessage: Data = byteArrayOf(
            0x0a, 0x0f, 0x6f, 0x72, 0x67, 0x2e, 0x65, 0x78, 0x61, 0x6d, 0x70, 0x6c, 0x65, 0x2e, 0x66,
            0x6f, 0x6f, 0x10, 0xab.toByte(), 0x02
        )

        val extendedError = decoder.decode(
            RawResponseMessage(
                responseCode = ResponseCode.serviceUnavailable,
                options = Options(),
                data = extendedErrorWithNullMessage
            )
        )

        assertEquals("org.example.foo", extendedError?.namespace)
        assertEquals(-150, extendedError?.code)
        assertNull(extendedError?.message)
    }

    @Test
    fun shouldDecodeExtendedErrorWithNoErrorAndMessageField() {
        val extendedErrorWithNullMessage: Data = byteArrayOf(
            0x0a, 0x0f, 0x6f, 0x72, 0x67, 0x2e, 0x65, 0x78, 0x61, 0x6d, 0x70, 0x6c, 0x65, 0x2e, 0x66,
            0x6f, 0x6f, 0x10, 0xab.toByte()
        )

        val extendedError = decoder.decode(
            RawResponseMessage(
                responseCode = ResponseCode.serviceUnavailable,
                options = Options(),
                data = extendedErrorWithNullMessage
            )
        )

        assertEquals("org.example.foo", extendedError?.namespace)
        assertEquals(0, extendedError?.code)
        assertNull(extendedError?.message)
    }

    @Test
    fun shouldReturnNullExtendedErrorForOkResponseCode() {
        val extendedError = decoder.decode(
            RawResponseMessage(
                responseCode = ResponseCode.created,
                options = Options(),
                data = extendedError
            )
        )

        assertNull(extendedError)
    }

    @Test
    fun shouldReturnDefaultExtendedErrorForEmptyBody() {
        val extendedError = decoder.decode(
            RawResponseMessage(
                responseCode = ResponseCode.serviceUnavailable,
                options = Options(),
                data = ByteArray(0)
            )
        )

        assertDefaultExtendedError(extendedError)
    }

    @Test
    fun shouldReturnDefaultExtendedErrorForInvalidBody() {
        val extendedError = decoder.decode(
            RawResponseMessage(
                responseCode = ResponseCode.serviceUnavailable,
                options = Options(),
                data = ByteArray(0x01)
            )
        )

        assertDefaultExtendedError(extendedError)
    }

    private fun assertDefaultExtendedError(extendedError: ExtendedError?) {
        assertNull(extendedError?.namespace)
        assertEquals(0, extendedError?.code)
        assertNull(extendedError?.message)
    }

}

// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bindings.util

import org.junit.Assert
import org.junit.Test
import org.junit.runner.RunWith
import org.junit.runners.JUnit4

class StringExtKtTest {
    @Test
    fun testConvertHexStringToByteArray() {
        val hexString = "0102030405060708090AF1F2F3F4F5F6"
        val referenceByteArray = byteArrayOf(
                0x01, 0x02, 0x03, 0x04, 0x05,
                0x06, 0x07, 0x08, 0x09, 0x0A,
                0xF1.toByte(), 0xF2.toByte(),
                0xF3.toByte(), 0xF4.toByte(),
                0xF5.toByte(), 0xF6.toByte()
        )

        var result = hexString.hexStringToByteArray()
        Assert.assertArrayEquals(referenceByteArray, result)

        result = hexString.toLowerCase().hexStringToByteArray()
        Assert.assertArrayEquals(referenceByteArray, result)
    }
}

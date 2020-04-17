// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bt.gatt.util

import org.junit.Assert
import org.junit.Test

import java.util.UUID

class UuidHelperKtTest {

    private val uuidByteArray = byteArrayOf(-51, -82, -43, 109, -121, 18, 65, 77, -77, 70, 1, -112, 93, 0, 38, -2)
    private val uuidString = "cdaed56d-8712-414d-b346-01905d0026fe"

    @Test
    fun should_Get_16_Bytes_From_a_UUID() {
        Assert.assertEquals("Expected result to be a byte array with 16 elements.", 16, uuidByteArray.size.toLong())
    }

    @Test
    fun should_Reconstruct_Same_UUID_From_Byte_Array() {
        val uuid = UUID.fromString(uuidString)

        val reconstructedUuid = uuidByteArray.toUuid()

        Assert.assertEquals(uuid, reconstructedUuid)
    }

    @Test
    fun should_Not_Generate_the_Same_UUID_From_Bytes() {
        val uuid = UUID.fromString(uuidString)

        val newUuid = UUID.nameUUIDFromBytes(uuidByteArray)
        val sameUuidFromHelper = uuidByteArray.toUuid()

        Assert.assertFalse(uuid == newUuid)
        Assert.assertFalse(sameUuidFromHelper == newUuid)
        Assert.assertTrue(sameUuidFromHelper == uuid)
    }
}

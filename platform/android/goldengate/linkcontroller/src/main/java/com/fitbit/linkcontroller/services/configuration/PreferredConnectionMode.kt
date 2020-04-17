// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.linkcontroller.services.configuration

/**
 * This is used to set a Preferred mode for the connection with the peripheral.
 * It is used within a LinkController that handles ble characteristic communication
 */
enum class PreferredConnectionMode(private val byte: Byte) {
    FAST(0x0),
    SLOW(0x1);


    fun toByteArray(): ByteArray {
        val byteArray = ByteArray(1)
        byteArray[0] = byte
        return byteArray
    }
}

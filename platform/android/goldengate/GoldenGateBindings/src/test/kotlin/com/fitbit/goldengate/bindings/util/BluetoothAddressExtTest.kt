// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bindings.util

import org.junit.Assert.*
import org.junit.Test

class BluetoothAddressExtTest {

    @Test
    fun shouldReturnTrueForValidBluetoothAddress() {
        assertTrue(checkBluetoothAddress("00:43:A8:23:10:F0"))
    }

    @Test
    fun shouldReturnFalseForInValidBluetoothAddress() {
        assertFalse(checkBluetoothAddress("GG:43:A8:23:10:F0"))
        assertFalse(checkBluetoothAddress("00:GG:A8:23:10:F0"))
    }

    @Test
    fun shouldReturnFalseForBluetoothAddressWithWrongFormat() {
        assertFalse(checkBluetoothAddress("aa:43:A8:23:10:F0"))
        assertFalse(checkBluetoothAddress("0043A82310F0"))
        assertFalse(checkBluetoothAddress(":00:43:A8:23:10:F0"))
        assertFalse(checkBluetoothAddress("0043:A8:23:10:F0"))
        assertFalse(checkBluetoothAddress("00:43:A8:23:10"))
        assertFalse(checkBluetoothAddress("0:0:43:A8:23:10:F0"))
        assertFalse(checkBluetoothAddress("00:43:A8:23:10F0"))
        assertFalse(checkBluetoothAddress("00-43-A8-23-10-F0"))
    }

    @Test
    fun shouldReturnFalseForNullBluetoothAddress() {
        assertFalse(checkBluetoothAddress(null))
    }

    @Test
    fun shouldReturnFalseForEmptyBluetoothAddress() {
        assertFalse(checkBluetoothAddress(""))
    }

}

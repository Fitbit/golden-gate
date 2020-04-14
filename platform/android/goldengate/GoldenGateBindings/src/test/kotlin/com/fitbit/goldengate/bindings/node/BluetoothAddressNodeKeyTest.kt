package com.fitbit.goldengate.bindings.node

import org.junit.Assert.assertNotNull
import org.junit.Test

const val valid_bluetooth_address = "00:43:A8:23:10:F0"

class BluetoothAddressNodeKeyTest {

    @Test
    fun shouldCreateObjectIfValidAddressIsProvided() {
        assertNotNull(BluetoothAddressNodeKey(valid_bluetooth_address))
    }

    @Test(expected = IllegalArgumentException::class)
    fun shouldThrowErrorIfEmptyAddressIsProvided() {
        BluetoothAddressNodeKey("")
    }

    @Test(expected = IllegalArgumentException::class)
    fun shouldThrowErrorIfInvalidAddressIsProvided() {
        BluetoothAddressNodeKey("adasasdas")
    }
}

// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bindings.coap

import com.fitbit.goldengate.bindings.BaseTest
import com.fitbit.goldengate.bindings.node.BluetoothAddressNodeKey
import com.fitbit.goldengate.bindings.node.valid_bluetooth_address
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Before
import org.junit.Test

class CoapEndpointProviderTest : BaseTest() {

    @Before
    fun setup() {
        CoapEndpointProvider.endpointMap.clear()
    }

    @Test
    fun shouldCreateAndReturnEndpointInstance() {
        assertNotNull(CoapEndpointProvider.getEndpoint(BluetoothAddressNodeKey(valid_bluetooth_address)))
    }

    @Test
    fun shouldReturnSameEndpointInstanceForSameKeyObject() {
        val nodeKey = BluetoothAddressNodeKey(valid_bluetooth_address)
        val endPoint1 = CoapEndpointProvider.getEndpoint(nodeKey)
        val endPoint2 = CoapEndpointProvider.getEndpoint(nodeKey)
        assertEquals(endPoint1, endPoint2)
    }

    @Test
    fun shouldReturnSameEndpointInstanceForSameKeyValue() {
        val endPoint1 = CoapEndpointProvider.getEndpoint(BluetoothAddressNodeKey(valid_bluetooth_address))
        val endPoint2 = CoapEndpointProvider.getEndpoint(BluetoothAddressNodeKey(valid_bluetooth_address))
        assertEquals(endPoint1, endPoint2)
    }
}

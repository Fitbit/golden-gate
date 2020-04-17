// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.linkcontroller

import com.fitbit.linkcontroller.services.status.CurrentConnectionStatus
import org.junit.Test
import kotlin.test.assertEquals

class CurrentConnectionStatusTest {

    @Test
    fun parseCurrentConfigurationTest() {
        val array = byteArrayOf(0x0F, 0x1B, 0x48, 0x01, 0xFB.toByte(), 0x48,0x08)
        val configuration = CurrentConnectionStatus.parseFromByteArray(array)

        assertEquals(true, configuration.bonded, " Invalid Bond Status")
        assertEquals(true, configuration.encrypted, " Invalid Encrypted Status")
        assertEquals(true, configuration.dleEnabled, " Invalid dle Enabled Status")
        assertEquals(true, configuration.dleReboot, " Invalid dle Reboot Status")

        assertEquals(0x1B, configuration.maxTxPayload, " Invalid Tx Payload Length")
        assertEquals(0x148, configuration.maxTxTime, "Invalid TX Time")
        assertEquals(0xFB.toByte(), configuration.maxRxPayload, " Invalid Rx Payload Length")
        assertEquals(0x848, configuration.maxRxTime, "Invalid RX Time")

    }

}



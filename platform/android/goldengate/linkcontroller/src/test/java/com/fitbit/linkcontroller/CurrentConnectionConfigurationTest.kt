package com.fitbit.linkcontroller

import com.fitbit.linkcontroller.services.status.ConnectionConfigurationParseError
import com.fitbit.linkcontroller.services.status.ConnectionMode
import com.fitbit.linkcontroller.services.status.CurrentConnectionConfiguration
import org.junit.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith

class CurrentConnectionConfigurationTest {

    @Test
    fun parseCurrentConfigurationTest() {
        val array = byteArrayOf(6, 0, 2, 0, 100, 0, 185.toByte(), 0, 1)
        val configuration = CurrentConnectionConfiguration.parseFromByteArray(array)
        assertEquals(7.5f, configuration.interval, " Invalid Connection Interval")
        assertEquals(2, configuration.slaveLatency, " Invalid slave latency")
        assertEquals(1000, configuration.supervisionTimeout, " Invalid supervision timeout")
        assertEquals(185, configuration.mtu, "Invalid mtu")
        assertEquals(ConnectionMode.FAST, configuration.connectionMode, "Invalid Connection Mode")

    }

    @Test
    fun badConfigurationModeTest() {
        val array = ByteArray(9)
        array[8] = 3
        assertFailsWith<ConnectionConfigurationParseError> {
           CurrentConnectionConfiguration.parseFromByteArray(array)
        }


    }


}



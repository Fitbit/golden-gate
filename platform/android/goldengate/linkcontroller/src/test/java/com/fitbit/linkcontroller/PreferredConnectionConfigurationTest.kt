package com.fitbit.linkcontroller

import com.fitbit.linkcontroller.services.configuration.PreferredConnectionConfiguration
import org.junit.Test
import kotlin.test.assertFailsWith
import kotlin.test.assertTrue

class PreferredConnectionConfigurationTest {

    @Test
    fun testFastConnectionConfig() {
        val builder = PreferredConnectionConfiguration.Builder()
        builder.setFastModeConfig(1000f, 1000f, 0, 6000)
        val array = builder.build().toByteArray()
        val expectedArray = ByteArray(18)
        expectedArray[0] = 1
        expectedArray[1] = 0x20
        expectedArray[2] = 0x03
        expectedArray[3] = 0x20
        expectedArray[4] = 0x03
        expectedArray[5] = 0
        expectedArray[6] = 0x3C
        assertTrue { array.contentEquals(expectedArray) }
    }

    @Test
    fun testSlowConnectionConfig() {
        val builder = PreferredConnectionConfiguration.Builder()
        builder.setSlowModeConfig(500f, 500f, 1, 6000)
        val array = builder.build().toByteArray()
        val expectedArray = ByteArray(18)
        expectedArray[0] = 2
        expectedArray[7] = 0x90.toByte()
        expectedArray[8] = 0x01
        expectedArray[9] = 0x90.toByte()
        expectedArray[10] = 0x01
        expectedArray[11] = 1
        expectedArray[12] = 0x3C
        assertTrue { array.contentEquals(expectedArray) }
    }

    @Test
    fun testDleConnectionConfig() {
        val builder = PreferredConnectionConfiguration.Builder()
        builder.setDLEConfig(100, 350)
        val array = builder.build().toByteArray()
        val expectedArray = ByteArray(18)
        expectedArray[0] = 4
        expectedArray[13] = 0x64
        expectedArray[14] = 0x5E
        expectedArray[15] = 0x01
        assertTrue { array.contentEquals(expectedArray) }
    }

    @Test
    fun testMTUConnectionConfig() {
        val builder = PreferredConnectionConfiguration.Builder()
        builder.setMtu(100)
        val array = builder.build().toByteArray()
        val expectedArray = ByteArray(18)
        expectedArray[0] = 8
        expectedArray[16] = 0x64

        assertTrue { array.contentEquals(expectedArray) }
    }

    @Test
    fun testMultipleConfigs() {
        val builder = PreferredConnectionConfiguration.Builder()
        builder
            .setFastModeConfig(1000f, 1000f, 0, 6000)
            .setSlowModeConfig(500f, 500f, 1, 6000)
            .setDLEConfig(100, 350)
            .setMtu(100)
            .requestDisconnect()

        val array = builder.build().toByteArray()
        val expectedArray = byteArrayOf(
            0x1F,
            0x20, 0x03, 0x20, 0x03, 0, 0x3c,
            0x90.toByte(), 0x01, 0x90.toByte(), 0x01, 0x01, 0x3c,
            0x64, 0x5e, 0x01,
            0x64, 0
        )

        assertTrue { array.contentEquals(expectedArray) }
    }

    @Test
    fun testInvalidConfiguration() {
        val builder = PreferredConnectionConfiguration.Builder()
        assertFailsWith<IllegalArgumentException> {
            builder.setFastModeConfig(10000f, 10000f, 0, 6000)
        }
        assertFailsWith<IllegalArgumentException> {
            builder.setFastModeConfig(0f, 500f, 0, 6000)
        }
        assertFailsWith<IllegalArgumentException> {
            builder.setFastModeConfig(10000f, 500f, 0, 6000)
        }
        assertFailsWith<IllegalArgumentException> {
            builder.setFastModeConfig(100f, 100f, 40, 6000)
        }
        assertFailsWith<IllegalArgumentException> {
            builder.setFastModeConfig(100f, 100f, 0, 8000)
        }

        assertFailsWith<IllegalArgumentException> {
            builder.setDLEConfig(252, 350)
        }
        assertFailsWith<IllegalArgumentException> {
            builder.setDLEConfig(26, 350)
        }
        assertFailsWith<IllegalArgumentException> {
            builder.setDLEConfig(100, 327)
        }
        assertFailsWith<IllegalArgumentException> {
            builder.setDLEConfig(100, 2121)
        }
        assertFailsWith<IllegalArgumentException> {
            builder.setMtu(22)
        }
        assertFailsWith<IllegalArgumentException> {
            builder.setMtu(507)
        }

    }
}

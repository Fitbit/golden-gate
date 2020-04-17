// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bindings.util

import com.fitbit.goldengate.bindings.BaseTest
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import java.util.Arrays

class MemoryDataSinkTest : BaseTest() {

    private val data = byteArrayOf(0x01)

    @Test
    fun shouldCreateSink() {
        MemoryDataSink().use { sink ->
            assertNotNull(sink)
            assertTrue(sink.thisPointer > 0)
        }
    }

    @Test
    fun canAttachDataSource() {
        MemoryDataSink().use { sink ->
            MemoryDataSource(data).use { source ->
                sink.attach(source.thisPointer)
            }
        }
    }

    @Test
    fun shouldReceiveDataSentFromSource() {
        MemoryDataSink().use { sink ->
            MemoryDataSource(data).use { source ->
                // attach DataSource that will send data
                sink.attach(source.thisPointer)
                // start sending data from DataSource
                source.start()

                // verify that sink gets the data sent from source
                assertTrue(Arrays.equals(sink.getBufferWithSleep(), data))
            }
        }
    }
}

/**
 * Sleep is added here as data in delivered on GG loop thread and synchronously checking can be
 * flaky without this sleep
 */
fun MemoryDataSink.getBufferWithSleep(millis: Long = 100): ByteArray {
    Thread.sleep(millis)
    return getBuffer()
}

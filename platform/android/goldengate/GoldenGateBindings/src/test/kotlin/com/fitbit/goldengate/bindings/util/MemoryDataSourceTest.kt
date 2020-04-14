package com.fitbit.goldengate.bindings.util

import com.fitbit.goldengate.bindings.BaseTest
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import java.util.Arrays

class MemoryDataSourceTest : BaseTest() {

    private val data = byteArrayOf(0x01)

    @Test
    fun shouldCreateSource() {
        MemoryDataSource(data).use { source ->
            assertNotNull(source)
            assertTrue(source.thisPointer > 0)
        }
    }

    @Test
    fun canAttachDataSink() {
        MemoryDataSource(data).use { source ->
            MemoryDataSink().use { sink ->
                source.attach(sink.thisPointer)

                // detach the source from this sink before it gets destroyed
                source.attach(0)
            }
        }
    }

    @Test
    fun shouldSendDataFromSourceToAttachedSink() {
        MemoryDataSource(data).use { source ->
            MemoryDataSink().use { sink ->
                // attach DataSink that will receive data
                source.attach(sink.thisPointer)

                // method to test (start sending data from DataSource)
                source.start()

                // verify that sink gets the data sent from source
                assertTrue(Arrays.equals(sink.getBufferWithSleep(), data))

                // detach the source from this sink before it gets destroyed
                source.attach(0)
            }
        }
    }
}

package com.fitbit.goldengate.bindings.io

import com.fitbit.goldengate.bindings.BaseTest
import com.fitbit.goldengate.bindings.mockDataSinkDataSource
import com.fitbit.goldengate.bindings.util.MemoryDataSink
import com.fitbit.goldengate.bindings.util.MemoryDataSource
import com.fitbit.goldengate.bindings.util.getBufferWithSleep
import org.junit.Assert.assertTrue
import org.junit.Test

class BlasterTest : BaseTest() {

    // data of size 3
    private val data = byteArrayOf(0x01, 0x02, 0x03)
    // we want to send one packet at a time
    private val chunkSize = 1

    @Test
    fun shouldDeliverBlastedPacketsToAttachedSink() {
        // create a blaster to send 10 packet of 100 bytes
        Blaster(packetSize = 100, sendInterval = 10).use { blaster ->
            MemoryDataSink().use { sink ->
                // attach stack (only need to attach DataSink)
                blaster.attach(mockDataSinkDataSource(dataSinkPtr = sink.thisPointer))

                // method to test
                blaster.start()

                // verify attached sink received all packets
                assertTrue(sink.getBufferWithSleep().isNotEmpty())

                // detach before the sink is destroyed
                blaster.detach()
            }
        }
    }

    @Test
    fun canStopBlaster() {
        // create a blaster to send 10 packet of 100 bytes
        Blaster(packetSize = 100, sendInterval = 10).use { blaster ->
            MemoryDataSink().use { sink ->
                // attach stack (only need to attach DataSink)
                blaster.attach(mockDataSinkDataSource(dataSinkPtr = sink.thisPointer))

                // start blasting
                blaster.start()

                // method under test
                blaster.stop()

                // verify attached sink has non-empty buffer
                assertTrue(sink.getBufferWithSleep().isNotEmpty())

                // detach before the sink is destroyed
                blaster.detach()
            }
        }
    }

    @Test
    fun shouldGetStatsIfDataIsSentFromAttachedDataSource() {
        Blaster().use { blaster ->
            MemoryDataSource(data, chunkSize).use { source ->
                // attach stack (only need to attach DataSource)
                blaster.attach(mockDataSinkDataSource(dataSourcePtr = source.thisPointer))

                // send data from stack (will send one packet at a time)
                source.start()

                // verify stats
                blaster.getStats()
                    .test()
                    .assertValue {
                        // Throughput starts counting when it receives first packet, hence packet count is one less than total packet sent
                        it.packetsReceived == ((data.size / chunkSize).toLong() - 1)
                    }

                // detach before the sink is destroyed
                blaster.detach()
            }
        }
    }

    @Test
    fun shouldGetEmptyStatsIfDataNotIsSentFromAttachedDataSource() {
        Blaster().use { blaster ->
            MemoryDataSource(data, chunkSize).use { source ->
                // attach stack (only need to attach DataSource)
                blaster.attach(mockDataSinkDataSource(dataSourcePtr = source.thisPointer))

                // verify that received data is of same size ZERO as data was NEVER sent
                blaster.getStats()
                    .test()
                    .assertValue { it.bytesReceived == 0L }

                // detach before the sink is destroyed
                blaster.detach()
            }
        }
    }

    @Test(expected = IllegalStateException::class)
    fun startShouldEmitErrorIfStackIsNotAttached() {
        Blaster().use { blaster ->
            blaster.start()
        }
    }

    @Test(expected = IllegalStateException::class)
    fun stopShouldEmitErrorIfStackIsNotAttached() {
        Blaster().use { blaster ->
            blaster.stop()
        }
    }

    @Test
    fun getStatsShouldEmitErrorIfStackIsNotAttached() {
        Blaster().use { blaster ->
            blaster.getStats()
                .test()
                .assertError { it is Exception }
        }
    }

    @Test
    fun detachShouldIgnoreIfStackIsNotAttached() {
        Blaster().use { blaster ->
            blaster.detach()
        }
    }

    @Test
    fun closeShouldIgnoreIfStackIsNotAttached() {
        val blaster = Blaster()
        blaster.close()
    }

}

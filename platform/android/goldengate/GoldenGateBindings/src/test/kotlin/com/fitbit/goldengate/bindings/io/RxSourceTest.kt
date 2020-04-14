package com.fitbit.goldengate.bindings.io

import com.fitbit.goldengate.bindings.BaseTest
import com.fitbit.goldengate.bindings.util.MemoryDataSink
import com.fitbit.goldengate.bindings.util.getBufferWithSleep
import org.junit.Assert
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertNotNull
import org.junit.Test
import java.util.Arrays

class RxSourceTest : BaseTest() {

    private val data = byteArrayOf(0x01)

    @Test
    fun assertRxSourceCreatedAndDestroyed() {
        RxSource().use { rxSource ->
            assertNotNull(rxSource)
            assertNotEquals(0, rxSource.thisPointer)
        }
    }

    @Test
    fun shouldGetDataOnAttachedSink() {
        MemoryDataSink().use { sink ->
            RxSource().use { rxSource ->
                // attach a in-memory sink to receive data for verification
                sink.attach(rxSource.thisPointer)

                // call to test
                rxSource.receiveData(data)
                    .test()
                    .assertComplete()

                // verify we got same data on in-memory sink
                Assert.assertTrue(Arrays.equals(sink.getBufferWithSleep(), data))
            }
        }
    }

    @Test
    fun shouldSilentlyFailIfDataWasReceivedButNoSinkAttached() {
        RxSource().use { rxSource ->
            // This call will try to put data but fail silently
            rxSource.receiveData(data)
                .test()
                .assertComplete()
        }
    }
}

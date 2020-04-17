// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bindings.io

import com.fitbit.goldengate.bindings.BaseTest
import com.fitbit.goldengate.bindings.mockDataSinkDataSource
import com.fitbit.goldengate.bindings.util.MemoryDataSink
import com.fitbit.goldengate.bindings.util.getBufferWithSleep
import org.junit.Assert
import org.junit.Test
import java.util.Arrays

class SingleMessageSenderTest : BaseTest() {

    private val data = byteArrayOf(0x01)

    @Test
    fun shouldSuccessfullySendDataAttachedStack() {
        SingleMessageSender().use { sender ->
            MemoryDataSink().use { sink ->
                // attach a in-memory sink to receive data for verification
                sender.attach(mockDataSinkDataSource(dataSinkPtr = sink.thisPointer))

                // call to test
                sender.send(data)
                    .test()
                    .assertComplete()

                // verify we got same data on in-memory sink
                Assert.assertTrue(Arrays.equals(sink.getBufferWithSleep(), data))

                // detach before the sink gets destroyed
                sender.detach()
            }
        }
    }

    @Test
    fun sendShouldEmitErrorIfStackIsNotAttached() {
        SingleMessageSender().use { sender ->
            sender.send(data)
                .test()
                .assertError { it is Exception }
        }
    }

    @Test
    fun detachShouldIgnoreIfStackIsNotAttached() {
        SingleMessageSender().use { sender ->
            sender.detach()
        }
    }

    @Test
    fun closeShouldIgnoreIfStackIsNotAttached() {
        SingleMessageSender().use { sender ->
            sender.close()
        }
    }

}

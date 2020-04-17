// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bindings.util

import com.fitbit.goldengate.bindings.NativeReference
import java.io.Closeable

/**
 * In-Memory DataSource that is intended to be used for testing
 *
 * @param data data that will be sent from this DataSource
 * @param chunkSize [start] call will send one packet for each chunkSize (default to send all data in one packet)
 */
class MemoryDataSource(
        data: ByteArray,
        chunkSize: Int = data.size
) : NativeReference, Closeable {

    override val thisPointer: Long

    init {
        thisPointer = create(data, chunkSize)
    }

    /**
     * Attach a DataSink that will receive data from this source
     */
    fun attach(dataSinkPtr: Long) = attach(thisPointer, dataSinkPtr)

    /**
     * Start sending the data from buffer to attached DataSink.
     *
     * This call will
     */
    fun start() = start(thisPointer)

    override fun close() = destroy(thisPointer)

    private external fun create(data: ByteArray, chunkSize: Int): Long
    private external fun destroy(selfPtr: Long)
    private external fun attach(selfPtr: Long, dataSinkPtr: Long)
    private external fun start(selfPtr: Long)

}

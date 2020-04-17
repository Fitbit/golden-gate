// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bindings.util

import com.fitbit.goldengate.bindings.NativeReference
import java.io.Closeable

/**
 * In-Memory DataSink that is intended to be used for testing
 */
class MemoryDataSink : NativeReference, Closeable {

    override val thisPointer: Long

    init {
        thisPointer = create()
    }

    /**
     * Attach a DataSource that will send data to this sink
     */
    fun attach(dataSourcePtr: Long) = attach(thisPointer, dataSourcePtr)

    /**
     * Get the in-memory buffer on which all data sent is available
     */
    fun getBuffer(): ByteArray = getBuffer(thisPointer)

    override fun close() = destroy(thisPointer)

    private external fun create(): Long
    private external fun destroy(selfPtr: Long)
    private external fun attach(selfPtr: Long, dataSourcePtr: Long)
    private external fun getBuffer(selfPtr: Long): ByteArray
}

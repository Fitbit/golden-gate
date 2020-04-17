// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bindings.io

import androidx.annotation.AnyThread
import com.fitbit.goldengate.bindings.GoldenGate
import com.fitbit.goldengate.bindings.NativeReference
import com.fitbit.goldengate.bindings.util.hexString
import io.reactivex.Completable
import timber.log.Timber
import java.io.Closeable

/**
 * RxSource is a DataSource for bottom on Golden Gate stack and DataSink for top of Native/Android
 * connection Bridge and is used to receive data packets to GG stack from connected Node
 */
class RxSource : NativeReference, Closeable {

    override val thisPointer: Long

    init {
        GoldenGate.check()
        thisPointer = create()
    }

    /**
     * Receive data from connected Node
     */
    @AnyThread
    fun receiveData(data: ByteArray): Completable {
        return Completable.fromCallable {
            // This jni call will post the message to the GG thread where the received data will be processed
            receiveData(data, thisPointer)
        }
    }

    override fun close() {
        destroy(thisPointer)
    }

    private external fun receiveData(data: ByteArray, rxSourcePtr: Long)
    private external fun create(): Long
    private external fun destroy(source: Long)
}

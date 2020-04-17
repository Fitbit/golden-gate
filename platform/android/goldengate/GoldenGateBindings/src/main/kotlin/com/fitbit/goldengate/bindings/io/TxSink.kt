// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bindings.io

import androidx.annotation.Keep
import androidx.annotation.VisibleForTesting
import com.fitbit.goldengate.bindings.GoldenGate
import com.fitbit.goldengate.bindings.NativeReference
import com.fitbit.goldengate.bindings.util.hexString
import io.reactivex.Observable
import io.reactivex.subjects.BehaviorSubject
import timber.log.Timber
import java.io.Closeable

/**
 * TxSink is a DataSink for bottom on Golden Gate stack and DataSource for top of Native/Android
 * connection Bridge and is used to transmit data packets from GG stack to connected Node
 */
class TxSink : NativeReference, Closeable {

    private val dataSubject = BehaviorSubject.create<ByteArray>()

    override val thisPointer: Long

    /**
     * Data to be transmitted (over tx characteristics) will be available on this Observable
     */
    val dataObservable: Observable<ByteArray>
        get() = dataSubject

    init {
        GoldenGate.check()
        thisPointer = create(TxSink::class.java, "putData", "([B)V")
    }

    /**
     * This should only be called from the JNI code and in tests as it is the output from Gattlink
     * to be sent over BLE
     */
    @VisibleForTesting
    @Keep
    fun putData(data: ByteArray) {
        dataSubject.onNext(data)
    }

    override fun close() {
        destroy(thisPointer)
    }

    private external fun create(clazz: Class<TxSink>, methodName: String, methodSignature: String): Long
    private external fun destroy(ptr: Long)
}

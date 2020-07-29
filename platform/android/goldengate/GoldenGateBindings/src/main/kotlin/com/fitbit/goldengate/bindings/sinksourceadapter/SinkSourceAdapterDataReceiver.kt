package com.fitbit.goldengate.bindings.sinksourceadapter

import io.reactivex.Observable
import io.reactivex.subjects.BehaviorSubject
import io.reactivex.subjects.Subject
import timber.log.Timber

/**
 * Receive Data from Binder API
 */
interface SinkSourceAdapterDataReceiver {

    fun receive(): Observable<ByteArray>

    fun receiveData(data: ByteArray)
}


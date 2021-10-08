// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bindings.coap.handler

import io.reactivex.Observable
import io.reactivex.Scheduler
import io.reactivex.internal.schedulers.TrampolineScheduler
import io.reactivex.schedulers.Schedulers
import io.reactivex.subjects.PublishSubject


/**
 * Abstract class extended from [BaseResourceHandler] with extra functionality to receive data
 * from a request and forward it to subscribed Observers
 *
 */
abstract class BaseRxResourceHandler : BaseResourceHandler() {

    private val updateSubject: PublishSubject<ByteArray> = PublishSubject.create()

    /**
     * Gets an Observable that will emit the data from the body of the incoming requests. The source
     * of the data(POST, PUT request etc.) depends on the implementation of the class that extends
     * [BaseRxResourceHandler]
     *
     * Emissions will be on [Schedulers.computation()]. If you wish to use another scheduler, use
     * the overload that takes a [Scheduler].
     *
     * @return An [Observable] that emits data as a ByteArray on [Schedulers.computation()].
     */
    fun observeUpdates(): Observable<ByteArray> = observeUpdates(Schedulers.computation())

    /**
     * Gets an Observable that will emit the data from the body of the incoming requests. The source
     * of the data(POST, PUT request etc.) depends on the implementation of the class that extends
     * [BaseRxResourceHandler]
     *
     * Emissions will be on [scheduler]. Passing a [TrampolineScheduler] is not allowed.
     *
     * @throws IllegalArgumentException if [scheduler] is a [TrampolineScheduler]
     *
     * @return An [Observable] that emits data as a ByteArray on [scheduler]
     */
    fun observeUpdates(scheduler: Scheduler): Observable<ByteArray> {
        if (scheduler is TrampolineScheduler) {
            throw IllegalArgumentException("Not allowed to use a TrampolineScheduler. This could result in blocking the Golden Gate thread.")
        }
        return updateSubject.hide().observeOn(scheduler)
    }

    /**
     * Instructs the internal [PublishSubject] to emit data. This method is intended to be used inside
     * handlers that extend the [BaseRxResourceHandler] when an [onPost] [onPut] is overridden
     */
     protected fun emitUpdates(data: ByteArray) = updateSubject.onNext(data)
}

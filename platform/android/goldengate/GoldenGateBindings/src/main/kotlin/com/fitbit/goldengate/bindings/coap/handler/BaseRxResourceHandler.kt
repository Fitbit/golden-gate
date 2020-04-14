package com.fitbit.goldengate.bindings.coap.handler

import io.reactivex.Observable
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
     * @return An [Observable] that emits data as a ByteArray
     */
    fun observeUpdates(): Observable<ByteArray> = updateSubject.hide()

    /**
     * Instructs the internal [PublishSubject] to emit data. This method is intended to be used inside
     * handlers that extend the [BaseRxResourceHandler] when an [onPost] [onPut] is override
     */
     protected fun emitUpdates(data: ByteArray) = updateSubject.onNext(data)
}
// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bindings.coap

import com.fitbit.goldengate.bindings.coap.data.Data
import com.fitbit.goldengate.bindings.coap.data.ExtendedError
import com.fitbit.goldengate.bindings.coap.data.ExtendedErrorDecoder
import com.fitbit.goldengate.bindings.coap.data.IncomingBody
import com.fitbit.goldengate.bindings.coap.data.IncomingResponse
import com.fitbit.goldengate.bindings.coap.data.Options
import com.fitbit.goldengate.bindings.coap.data.OutgoingRequest
import com.fitbit.goldengate.bindings.coap.data.RawResponseMessage
import com.fitbit.goldengate.bindings.coap.data.ResponseCode
import com.fitbit.goldengate.bindings.coap.data.error
import com.fitbit.goldengate.bindings.dtls.DtlsProtocolStatus.TlsProtocolState.TLS_STATE_UNKNOWN
import com.fitbit.goldengate.bindings.stack.Stack
import com.fitbit.goldengate.bindings.stack.StackEvent.Unknown
import io.reactivex.Single
import io.reactivex.SingleEmitter
import java.util.concurrent.atomic.AtomicBoolean
import timber.log.Timber
import java.lang.ref.WeakReference

/**
 * Response listener for single(non-blockwise) response message
 */
internal class SingleCoapResponseListener(
    private val request: OutgoingRequest,
    private val responseEmitter: SingleEmitter<IncomingResponse>,
    private val stack: WeakReference<Stack?>,
    private val errorDecoder: ExtendedErrorDecoder = ExtendedErrorDecoder()
) : CoapResponseListener {

    private var completed: Boolean = false
    private var nativeResponseListenerReference: Long = 0L
    private val isResponseObjectCleanUpNeeded = AtomicBoolean(true)

    override fun onAck() {
        // Just logging for now
        Timber.i("Received ACK for request: $request")
    }

    override fun onError(error: Int, message: String) {
        val exception = CoapEndpointException(
            message,
            error,
            lastDtlsState = stack.get()?.lastDtlsState?.value?: TLS_STATE_UNKNOWN.value,
            lastStackEvent = stack.get()?.lastStackEvent?.eventId?: Unknown.eventId
        )
        emitFailedCompletion(exception)
        // clean up native listener object
        cleanupNativeListener()
    }

    override fun onNext(message: RawResponseMessage) {
        if (message.responseCode.error() && request.expectSuccess) {
            val exception = CoapEndpointResponseException(message.responseCode, errorDecoder.decode(message.data))
            emitFailedCompletion(exception)
        } else {
            val response = createIncomingResponse(message)
            emitSuccessfulCompletion(response)
        }
        // clean up native listener object
        cleanupNativeListener()
    }

    override fun onComplete() {
        // no-op : callback to onNext is treated as onComplete
    }

    override fun isComplete() = completed

    override fun setNativeListenerReference(nativeReference: Long) {
        nativeResponseListenerReference = nativeReference
    }

    /**
     * Cancel and clean up native coap listener object
     */
    override fun cleanupNativeListener() {
        if (isResponseObjectCleanUpNeeded.getAndSet(false) && nativeResponseListenerReference != 0L) {
            cancelResponse(
                nativeResponseListenerReference
            )
        }
    }

    private fun createIncomingResponse(message: RawResponseMessage): IncomingResponse {
        return object : IncomingResponse {
            override val responseCode: ResponseCode
                get() = message.responseCode

            override val extendedError: ExtendedError?
                get() = errorDecoder.decode(message)

            override val options: Options
                get() = message.options

            override val body: IncomingBody
                get() = object : IncomingBody {
                    override fun asData(): Single<Data> {
                        return Single.just(message.data)
                    }
                }
        }
    }

    private fun emitFailedCompletion(throwable: Throwable) {
        completed = true
        responseEmitter.onError(throwable)
        request.progressObserver.onError(throwable)
    }

    private fun emitSuccessfulCompletion(response: IncomingResponse) {
        completed = true
        responseEmitter.onSuccess(response)
        request.progressObserver.onComplete()
    }

    private external fun cancelResponse(
        nativeResponseListenerReference: Long
    ): Int
}

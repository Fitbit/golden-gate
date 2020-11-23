// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bindings.coap.block

import com.fitbit.goldengate.bindings.coap.CoapEndpointException
import com.fitbit.goldengate.bindings.coap.CoapEndpointResponseException
import com.fitbit.goldengate.bindings.coap.CoapResponseListener
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
import io.reactivex.Single
import io.reactivex.SingleEmitter
import io.reactivex.subjects.BehaviorSubject
import timber.log.Timber

/**
 * Response listener for blockwise response message. This class will receive and parse a CoAP message that
 *
 * @param request The request this listener will listen for a response to
 * @param responseEmitter The sink which will receive the response or error passed to this class from the C layer
 */
internal class BlockwiseCoapResponseListener(
    private val request: OutgoingRequest,
    private val responseEmitter: SingleEmitter<IncomingResponse>,
    private val errorDecoder: ExtendedErrorDecoder = ExtendedErrorDecoder()
) : CoapResponseListener {

    private var completed: Boolean = false
    private var started = false
    private var data = Data(0)
    private var cancelable = {}

    private val bodyBehaviorSubject = BehaviorSubject.create<Data>()
    private val bodySingle = Single.fromObservable(bodyBehaviorSubject.take(1))

    override fun onAck() {
        // Just logging for now
        Timber.i("Received ACK for request: $request")
    }

    override fun onError(error: Int, message: String) {
        completed = true
        val exception = CoapEndpointException(message, error, data)
        // transmit the error to progressObserver
        request.progressObserver.onError(exception)
        if (!started) {
            // if first callback is error we emit error on response stream
            responseEmitter.onError(exception)
        } else {
            /*
             * If we get error callback after receiving first block, we emit error on response body
             * as at this point response is already completed
             */
            bodyBehaviorSubject.onError(exception)
        }
    }

    override fun onNext(message: RawResponseMessage) {
        this.data = this.data.plus(message.data)

        val exception = if (message.responseCode.error() && request.expectSuccess) {
            CoapEndpointResponseException(
                message.responseCode,
                errorDecoder.decode(message)
            )
        } else {
            null
        }
        if (!started) {
            started = true

            /**
             * TODO: FC-1525 - setting complete here means we can only cancel blockwise coap request
             * before first block is received and after first block is received canceling will not work.
             */
            completed = true

            if (exception != null) {
                request.progressObserver.onError(exception)
                responseEmitter.onError(exception)
            } else {
                // emit response completion
                val blockwiseResponse = createIncomingResponse(message)
                responseEmitter.onSuccess(blockwiseResponse)
            }
        } else {
            if (exception != null) {
                request.progressObserver.onError(exception)
                bodyBehaviorSubject.onError(exception)
            }
        }
    }

    override fun onComplete() {
        // emit onComplete on progress Observer
        request.progressObserver.onComplete()
        // emit response body
        bodyBehaviorSubject.onNext(data)
    }

    override fun isComplete() = completed

    /**
     * Sets a Cancellable for incoming coap response body stream
     */
    fun setCancellable(cancellable: () -> Unit) {
        this.cancelable = cancellable
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
                        return bodySingle
                            .doOnDispose {
                                cancelable()
                                Timber.d("cancel ongoing blockwise coap request")
                            }
                    }
                }
        }
    }
}

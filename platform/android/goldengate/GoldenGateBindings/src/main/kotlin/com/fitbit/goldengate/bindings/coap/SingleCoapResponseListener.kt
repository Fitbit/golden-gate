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
import io.reactivex.Single
import io.reactivex.SingleEmitter
import timber.log.Timber

/**
 * Response listener for single(non-blockwise) response message
 */
internal class SingleCoapResponseListener(
    private val request: OutgoingRequest,
    private val responseEmitter: SingleEmitter<IncomingResponse>,
    private val errorDecoder: ExtendedErrorDecoder = ExtendedErrorDecoder()
) : CoapResponseListener {

    private var completed: Boolean = false

    override fun onAck() {
        // Just logging for now
        Timber.i("Received ACK for request: $request")
    }

    override fun onError(error: Int, message: String) {
        val exception = CoapEndpointException(message, error)
        emitFailedCompletion(exception)
    }

    override fun onNext(message: RawResponseMessage) {
        if (message.responseCode.error() && request.expectSuccess) {
            val exception = CoapEndpointResponseException(message.responseCode, errorDecoder.decode(message.data))
            emitFailedCompletion(exception)
        } else {
            val response = createIncomingResponse(message)
            emitSuccessfulCompletion(response)
        }
    }

    override fun onComplete() {
        // no-op : callback to onNext is treated as onComplete
    }

    override fun isComplete() = completed

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

}

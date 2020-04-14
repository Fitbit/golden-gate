package com.fitbit.goldengatehost.coap.handler

import com.fitbit.goldengate.bindings.coap.data.ContentFormatOption
import com.fitbit.goldengate.bindings.coap.data.FormatOptionValue
import com.fitbit.goldengate.bindings.coap.data.IncomingRequest
import com.fitbit.goldengate.bindings.coap.data.OutgoingResponse
import com.fitbit.goldengate.bindings.coap.data.OutgoingResponseBuilder
import com.fitbit.goldengate.bindings.coap.handler.BaseResourceHandler
import io.reactivex.Single

/**
 * Coap resource handler that will return "Hello world!"
 */
class HelloWorldResourceHandler : BaseResourceHandler() {

    override fun onGet(request: IncomingRequest, responseBuilder: OutgoingResponseBuilder): Single<OutgoingResponse> {
        return Single.just(helloWordResponse(responseBuilder))
    }

    private fun helloWordResponse(responseBuilder: OutgoingResponseBuilder) =
            responseBuilder
                    .option(ContentFormatOption(FormatOptionValue.TEXT_PLAIN))
                    .body("Hello world!".toByteArray())
                    .build()
}

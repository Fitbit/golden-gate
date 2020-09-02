// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bindings.coap.handler

import com.fitbit.goldengate.bindings.coap.data.Block1Option
import com.fitbit.goldengate.bindings.coap.data.IncomingRequest
import com.fitbit.goldengate.bindings.coap.data.OptionNumber
import com.fitbit.goldengate.bindings.coap.data.OutgoingResponse
import com.fitbit.goldengate.bindings.coap.data.OutgoingResponseBuilder
import io.reactivex.Single
import timber.log.Timber

/**
 * Resource handler to echo back any PUT and POST request
 */
class EchoResourceHandler : BaseResourceHandler() {

    override fun onPut(request: IncomingRequest, responseBuilder: OutgoingResponseBuilder) =
        echo(request, responseBuilder)

    override fun onPost(request: IncomingRequest, responseBuilder: OutgoingResponseBuilder) =
        echo(request, responseBuilder)

    private fun echo(request: IncomingRequest, responseBuilder: OutgoingResponseBuilder): Single<OutgoingResponse> {
        return request.body.asData()
            .map { data ->
                Timber.i("Request body size: ${data.size}")
                responseBuilder
                    .addAll(request.options)
                    .body(data)
                    .build()
            }
    }
}

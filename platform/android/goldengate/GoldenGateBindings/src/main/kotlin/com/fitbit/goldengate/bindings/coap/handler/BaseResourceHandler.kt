// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bindings.coap.handler

import com.fitbit.goldengate.bindings.coap.data.IncomingRequest
import com.fitbit.goldengate.bindings.coap.data.OutgoingResponse
import com.fitbit.goldengate.bindings.coap.data.OutgoingResponseBuilder
import com.fitbit.goldengate.bindings.coap.data.ResponseCode
import io.reactivex.Single

/**
 * Base Implementation for [ResourceHandler] providing request not supported implementation
 * for all resource handler.
 */
open class BaseResourceHandler : ResourceHandler {

    override fun onGet(request: IncomingRequest, responseBuilder: OutgoingResponseBuilder) =
            methodNotAllowedResponse(responseBuilder)

    override fun onPost(request: IncomingRequest, responseBuilder: OutgoingResponseBuilder) =
            methodNotAllowedResponse(responseBuilder)

    override fun onPut(request: IncomingRequest, responseBuilder: OutgoingResponseBuilder) =
            methodNotAllowedResponse(responseBuilder)

    override fun onDelete(request: IncomingRequest, responseBuilder: OutgoingResponseBuilder) =
            methodNotAllowedResponse(responseBuilder)

    private fun methodNotAllowedResponse(responseBuilder: OutgoingResponseBuilder): Single<OutgoingResponse> =
            Single.just(
                    responseBuilder
                            .responseCode(ResponseCode.methodNotAllowed)
                            .build()
            )
}

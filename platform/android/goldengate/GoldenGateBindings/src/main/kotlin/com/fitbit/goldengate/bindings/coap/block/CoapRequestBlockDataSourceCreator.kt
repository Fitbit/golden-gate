// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bindings.coap.block

import com.fitbit.goldengate.bindings.coap.data.BytesArrayOutgoingBody
import com.fitbit.goldengate.bindings.coap.data.EmptyOutgoingBody
import com.fitbit.goldengate.bindings.coap.data.FileUriOutgoingBody
import com.fitbit.goldengate.bindings.coap.data.InputStreamOutgoingBody
import com.fitbit.goldengate.bindings.coap.data.Method
import com.fitbit.goldengate.bindings.coap.data.OutgoingBody
import com.fitbit.goldengate.bindings.coap.data.OutgoingRequest

/**
 * Helper called from JNI to provide a convenient way of creating [BlockDataSource] for CoAP request
 */
internal class CoapRequestBlockDataSourceCreator {

    /**
     * Create a new instance of [BlockDataSource] from given message
     *
     * @param request outgoing coap request message
     * @return new instance of [BlockDataSource] if request method is supported, throws Exception
     */
    fun create(request: OutgoingRequest): BlockDataSource? {
        return when (request.method) {
            Method.GET -> sourceWithRequiredEmptyBody(request.body)
            Method.POST -> sourceWithOptionalBodyFromRequest(request)
            Method.PUT -> sourceWithOptionalBodyFromRequest(request)
            Method.DELETE -> sourceWithRequiredEmptyBody(request.body)
        }
    }

    private fun sourceWithRequiredEmptyBody(body: OutgoingBody): BlockDataSource? {
        require(body is EmptyOutgoingBody) {
            "Sending Body is only supported by PUT and POST methods"
        }
        return null
    }

    private fun sourceWithOptionalBodyFromRequest(request: OutgoingRequest): BlockDataSource {
        val body = request.body
        return when (body) {
            is InputStreamOutgoingBody -> InputStreamBlockDataSource(
                body.data,
                request.progressObserver
            )
            is BytesArrayOutgoingBody -> BytesArrayBlockDataSource(
                body.data,
                request.progressObserver
            )
            is FileUriOutgoingBody -> FileUriBlockDataSource(
                body.data,
                request.progressObserver
            )
            is EmptyOutgoingBody -> BytesArrayBlockDataSource(body.data, request.progressObserver)
        }
    }
}

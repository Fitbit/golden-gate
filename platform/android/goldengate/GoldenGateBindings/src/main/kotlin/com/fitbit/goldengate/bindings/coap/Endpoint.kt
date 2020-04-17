// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bindings.coap

import com.fitbit.goldengate.bindings.coap.data.IncomingResponse
import com.fitbit.goldengate.bindings.coap.data.Method
import com.fitbit.goldengate.bindings.coap.data.OutgoingRequest
import com.fitbit.goldengate.bindings.coap.data.OutgoingRequestBuilder
import com.fitbit.goldengate.bindings.coap.handler.ResourceHandler
import io.reactivex.Completable
import io.reactivex.Single

/**
 * This class is the Client/Server API of that will be exposed to users of this library.
 *
 * Example:
 *
 * ```
 * fun sendMegadump(megadump: ByteArray): Single<IncomingResponse> {
 *     val request = OutgoingRequestBuilder("/sync/response", Method.PUT).body(megadump).build()
 *     return endpoint.responseFor(request)
 * }
 * ```
 */
interface Endpoint {
    /**
     * Call this to send a request and get a response (i.e. be a client).
     *
     * @param request The request that should be sent to the CoAP server. Use [OutgoingRequestBuilder] to construct requests.
     *
     * @return A [Single] that will emit either an [IncomingResponse] returned from the CoAP server or an error
     */
    fun responseFor(request: OutgoingRequest): Single<IncomingResponse>

    /**
     * Registers a coap handler for a certain path.
     *
     * An error will be emitted if trying to register handler on previously registered [path]
     *
     * @param path The path where the coap handler will be registered.
     * @param handler The handler that will get called when a coap request is received.
     * @param configuration contains all the flags used by coap handler registration
     * @return Completable
     *      - Error: If the coap endpoint registration fails.
     *      - Complete: The coap handler is registered.
     */
    fun addResourceHandler(path: String, handler: ResourceHandler,
                           configuration: CoapEndpointHandlerConfiguration = CoapEndpointHandlerConfiguration()): Completable

    /**
     * Unregisters the coap handler for a certain path
     *
     * @param path The path where the coap handler will be unregistered.
     */
    fun removeResourceHandler(path: String): Completable

    /**
     * Convenience method for parsing a Response into a T
     *
     * @param T The type of response object expected from this request
     * @param request The request to send over CoAP.
     * @param parser An object that knows how to construct an object of type [T] from a ByteArray
     *
     * @return A [Single] that will emit either the response parsed into a [T] or an error.
     */
    fun <T> responseFor(request: OutgoingRequest, parser: ByteArrayParser<T>): Single<T> {
        return responseFor(request)
                .flatMap { it.body.asData() }
                .flatMap { parser.parse(it) }
    }

    /**
     * Convenience method for making a GET request to path and parsing it into a T
     *
     * @param T The type of object expected to be returned from a GET request to [path]
     * @param path The path to make a request for over CoAP
     * @param parser An object that knows how to construct an object of type [T] from a ByteArray
     *
     * @return A [Single] that will emit either the response parsed into a [T] or an error.
     */
    fun <T> resource(path: String, parser: ByteArrayParser<T>): Single<T> {
        val request = OutgoingRequestBuilder(path, Method.GET).build()
        return responseFor(request, parser)
    }
}

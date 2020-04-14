package com.fitbit.goldengate.bindings.coap.handler

import com.fitbit.goldengate.bindings.coap.data.IncomingRequest
import com.fitbit.goldengate.bindings.coap.data.OutgoingResponse
import com.fitbit.goldengate.bindings.coap.data.OutgoingResponseBuilder
import io.reactivex.Single

/**
 * A user of this API will implement this interface and register it with an Endpoint.
 * The user can then expect each of these methods to be called when an appropriate request is received.
 *
 * Example:
 *
 * ```
 * class SimpleResourceHandler : ResourceHandler {
 *     override fun onGet(request: IncomingRequest, responseBuilder: OutgoingResponseBuilder): Single<OutgoingResponse> {
 *         //examine request if desired
 *         val bodyData: ByteArray = ....
 *         return Single.just(
 *                 responseBuilder.responseCode(ResponseCode(2, 5)).body(BytesArrayOutgoingBody(bodyData)).build()
 *         )
 *     }
 *
 *     override fun onPost(request: IncomingRequest, responseBuilder: OutgoingResponseBuilder): Single<OutgoingResponse> {
 *         val bodyData = request.body.asData.blockingGet()
 *         //process body data.
 *         return Single.just(
 *                 responseBuilder.responseCode(ResponseCode(2, 1)).build()
 *         )
 *     }
 *
 *     override fun onPut(request: IncomingRequest, responseBuilder: OutgoingResponseBuilder): Single<OutgoingResponse> {
 *         val bodyData = request.body.asData.blockingGet()
 *         //process body data.
 *         return Single.just(
 *                 responseBuilder.responseCode(ResponseCode(2, 4)).build()
 *         )
 *     }
 *
 *     override fun onDelete(request: IncomingRequest, responseBuilder: OutgoingResponseBuilder): Single<OutgoingResponse> {
 *         return Single.just(
 *                 responseBuilder.responseCode(METHOD_NOT_ALLOWED_RESPONSE_CODE).build()
 *         )
 *     }
 * }
 * ```
 */
interface ResourceHandler {

    /**
     * Handler for request of GET
     *
     * @param request the incoming request. Examine its options to view query parameters and other metadata.
     * @param responseBuilder the [OutgoingResponseBuilder] that should be used to construct a response.
     *
     * @return A [Single] that emits either an [OutgoingResponse] built with [responseBuilder] or an error. This response will be
     * delivered to the client.
     */
    fun onGet(request: IncomingRequest, responseBuilder: OutgoingResponseBuilder): Single<OutgoingResponse>

    /**
     * Handler for request of POST
     *
     * @param request the incoming request. Metadata is available in its options, and the body contains the data sent
     * with the POST request
     * @param responseBuilder the [OutgoingResponseBuilder] that should be used to construct a response.
     *
     * @return A [Single] that emits either an [OutgoingResponse] built with [responseBuilder] or an error. This response will be
     * delivered to the client.
     */
    fun onPost(request: IncomingRequest, responseBuilder: OutgoingResponseBuilder): Single<OutgoingResponse>

    /**
     * Handler for request of PUT
     *
     * @param request the incoming request. Metadata is available in its options, and the body contains the data sent
     * with the POST request
     * @param responseBuilder the [OutgoingResponseBuilder] that should be used to construct a response.
     *
     * @return A [Single] that emits either an [OutgoingResponse] built with [responseBuilder] or an error. This response will be
     * delivered to the client.
     */
    fun onPut(request: IncomingRequest, responseBuilder: OutgoingResponseBuilder): Single<OutgoingResponse>

    /**
     * Handler for request of DELETE
     *
     * @param request the incoming request. Examine its options to view query parameters and other metadata.
     * @param responseBuilder the [OutgoingResponseBuilder] that should be used to construct a response.
     *
     * @return A [Single] that emits either an [OutgoingResponse] built with [responseBuilder] or an error. This response will be
     * delivered to the client.
     */
    fun onDelete(request: IncomingRequest, responseBuilder: OutgoingResponseBuilder): Single<OutgoingResponse>
}

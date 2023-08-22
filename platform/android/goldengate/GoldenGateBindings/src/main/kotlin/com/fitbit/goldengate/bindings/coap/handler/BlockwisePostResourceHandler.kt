package com.fitbit.goldengate.bindings.coap.handler

import com.fitbit.goldengate.bindings.coap.data.IncomingRequest
import com.fitbit.goldengate.bindings.coap.data.OutgoingResponse
import com.fitbit.goldengate.bindings.coap.data.OutgoingResponseBuilder
import io.reactivex.Single

/**
 * Implementation of resource handler to handle the request with block1 option with Post method
 * The subclass needs to implement [BlockwiseBlock1Server] to receive blockwise request payload
 *
 * @thread GG Loop
 */
abstract class BlockwisePostResourceHandler(
    private val helper: Block1RequestProcessHelper = Block1RequestProcessHelper()
): BaseResourceHandler(), BlockwiseBlock1Server {

    override fun onPost(request: IncomingRequest, responseBuilder: OutgoingResponseBuilder): Single<OutgoingResponse> {
        return request.body.asData()
            .map { data ->
                helper.processBlock1Request(this, request.options, data, responseBuilder)
            }
    }
}

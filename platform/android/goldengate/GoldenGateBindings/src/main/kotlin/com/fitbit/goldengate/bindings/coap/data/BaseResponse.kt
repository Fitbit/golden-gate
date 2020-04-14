package com.fitbit.goldengate.bindings.coap.data

/**
 * Holds common data for [IncomingResponse] and [OutgoingResponse]
 */
interface BaseResponse {

    /**
     * CoAP response code
     */
    val responseCode: ResponseCode
}

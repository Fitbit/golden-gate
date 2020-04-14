package com.fitbit.goldengate.bindings.coap.data

/**
 * Incoming coap response message.
 *
 * Coap client will receive this response object as a result of requesting data from server
 */
interface IncomingResponse : IncomingMessage, BaseResponse {

    /**
     * Extended error information, used when [ResponseCode] is
     * [ResponseCode.Class.clientError] or [ResponseCode.Class.serverError]
     */
    val extendedError: ExtendedError?

}

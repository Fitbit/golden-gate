package com.fitbit.goldengate.bindings.coap.handler

import com.fitbit.goldengate.bindings.coap.data.Data
import com.fitbit.goldengate.bindings.coap.data.Options
import com.fitbit.goldengate.bindings.coap.data.OutgoingResponse

/**
 * Resource handler implements this interface to receive blockwise transfer for Block1 option, which can
 * contain large payload in CoAP request.
 */
interface BlockwiseBlock1Server {
    /**
     * This resource handler callback will be invoked when the first block of blockwise request has
     * been received
     *
     * @param options The option list from the request
     *
     * @thread GG Loop
     */
    fun onStart(options: Options)

    /**
     * This resource handler callback will be invoked when each block of blockwise request has been
     * received.
     *
     * If the resource handler has identified any issues (i.g. data validation failure) and
     * would like to cancel the blockwise request. It can construct an OutgoingResponse with error
     * code. In this early termination scenario, onEnd() might not be invoked if it is not the last
     * block. Resource handler should reset the internal state before it returns the response.
     *
     * In normal scenario, it can just return null, and gg xp lib will autogenerate the 2.31(Continue)
     * blockwise response for the resource handler.
     *
     * @param data The block data in request body
     * @param options The option list from the request
     *
     * @return null if no customized response is needed
     *         otherwise, OutgoingResponse object can be returned
     *         If this is the last block, the return object from onEnd() will be used instead and
     *         this return object will be ignored
     *
     * @thread GG Loop
     */
    fun onData(options: Options, data: Data): OutgoingResponse?

    /**
     * This resource handler callback will be invoked when the last block of blockwise request has been
     * received
     *
     * @param options The option list from the request
     * @param result reports block1 request status from [Block1ProcessResult]
     *
     *
     * @return OutgoingResponse object if resource handler would like to customize the response
     *         message field (adding error code or new option field)
     *         if it returns null, gg xp lib will autogenerate a 2.04 response
     *
     * @thread GG Loop
     */
    fun onEnd(options: Options, result: Block1ProcessResult): OutgoingResponse?
}


package com.fitbit.goldengate.bindings.coap.data

/**
 * Concrete class of MessageBuilder for building [OutgoingResponse]
 */
class OutgoingResponseBuilder : BaseOutgoingMessageBuilder<OutgoingResponse>() {

    internal var responseCode = ResponseCode.created

    /**
     * Set the [ResponseCode] to build a response with (e.g. 2.01, 4.04)
     *
     * @param code the [ResponseCode] this builder will build an [OutgoingResponse] with.
     */
    fun responseCode(code: ResponseCode): OutgoingResponseBuilder {
        responseCode = code
        return this
    }

    override fun build(): OutgoingResponse {
        return object : OutgoingResponse {
            override val responseCode: ResponseCode
                get() = this@OutgoingResponseBuilder.responseCode

            override val body: OutgoingBody
                get() = this@OutgoingResponseBuilder.body

            override val options: Options
                get() = this@OutgoingResponseBuilder.options
        }
    }
}

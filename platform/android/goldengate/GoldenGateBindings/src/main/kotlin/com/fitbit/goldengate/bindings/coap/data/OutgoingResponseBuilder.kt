// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bindings.coap.data

/**
 * Concrete class of MessageBuilder for building [OutgoingResponse]
 */
class OutgoingResponseBuilder : BaseOutgoingMessageBuilder<OutgoingResponse>() {

    internal var responseCode = ResponseCode.created

    internal var forceNonBlockwise = false

    /**
     * Set the [ResponseCode] to build a response with (e.g. 2.01, 4.04)
     *
     * @param code the [ResponseCode] this builder will build an [OutgoingResponse] with.
     */
    fun responseCode(code: ResponseCode): OutgoingResponseBuilder {
        responseCode = code
        return this
    }

    /**
     * Flag to send the response as non-blockwise. In a blockwise request data is sent in chuncks(blocks)
     * of fixed size. In GG we use 1024 bytes. For the AOSP peripheral role, some Coap resources accept
     * larger data payloads in a single block. In this case we need to send a non-blockwise response.
     */
    fun forceNonBlockwise(value: Boolean): OutgoingResponseBuilder {
        forceNonBlockwise = value
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

            override val forceNonBlockwise: Boolean
                get() = this@OutgoingResponseBuilder.forceNonBlockwise
        }
    }
}

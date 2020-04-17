// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bindings.coap.data

import io.reactivex.Observer

private const val PATH_DELIMITER = "/"

/**
 * Concrete class of MessageBuilder for building [OutgoingRequest]
 *
 * @property path The path to send this request to. (e.g. /foo/bar)
 * @property method The CoAP Method to use for this request (GET/PUT/POST/DELETE)
 */
class OutgoingRequestBuilder(
        val path: String,
        val method: Method
) : BaseOutgoingMessageBuilder<OutgoingRequest>() {

    internal var expectSuccess = false

    internal var forceNonBlockwise = false

    internal var maxResendCount = -1 // invalid number: use default value from gg lib

    internal var ackTimeout = 0 // 0: use default value from gg lib

    init {
        addPathToOptions(path)
    }

    /**
     * Tweaks Observable behavior and allows for custom error handling.
     * If true, a non-2.xx response will error the response() Observable.
     * If false (default), a non-2.xx response will be reported as a `next` event.
     */
    fun expectSuccess(expect: Boolean): OutgoingRequestBuilder {
        expectSuccess = expect
        return this
    }

    /**
     * Flag to treat the request as non-blockwise. In a blockwise request data is sent in chuncks(blocks)
     * of fixed size. In GG we use 1024 bytes. On the tracker some Coap resources accept larger data payloads
     * in a single block. In this case we need to force a non-blockwise request. Here is an example of this
     * type of resource:
     *
     * https://wiki.fitbit.com/pages/viewpage.action?pageId=109586801
     */
    fun forceNonBlockwise(value: Boolean): OutgoingRequestBuilder {
        forceNonBlockwise = value
        return this
    }

    /**
     * Config the maximum number of times the client will resend the request if there is a response timeout.
     * When set to 0, a request will only be sent once and not re-sent.
     * When set to a negative number(e.g. -1), the max resend count will set to [GG_COAP_DEFAULT_MAX_RETRANSMIT]
     * from GG XP lib
     */
    fun maxResendCount(value: Int): OutgoingRequestBuilder {
        maxResendCount = value
        return this
    }

    /**
     * Config the timeout after which a resend will happen, in milliseconds.
     * When set to 0, to use a default value according to the CoAP specification and the endpoint.
     */
    fun ackTimeout(value: Int): OutgoingRequestBuilder {
        ackTimeout = value
        return this
    }

    override fun build(): OutgoingRequest {
        return object : OutgoingRequest {
            override val method: Method
                get() = this@OutgoingRequestBuilder.method

            override val expectSuccess: Boolean
                get() = this@OutgoingRequestBuilder.expectSuccess

            override val forceNonBlockwise: Boolean
                get() = this@OutgoingRequestBuilder.forceNonBlockwise

            override val body: OutgoingBody
                get() = this@OutgoingRequestBuilder.body

            override val options: Options
                get() = this@OutgoingRequestBuilder.options

            override val progressObserver:Observer<Int>
                get() = this@OutgoingRequestBuilder.progressObserver

            override val maxResendCount: Int
                get() = this@OutgoingRequestBuilder.maxResendCount

            override val ackTimeout: Int
                get() = this@OutgoingRequestBuilder.ackTimeout
        }
    }

    private fun addPathToOptions(path: String) {
        path.split(PATH_DELIMITER).forEach { subPath ->
            if (!subPath.isBlank()) {
                option(UriPathOption(subPath))
            }
        }
    }
}

// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bindings.coap.data

/**
 * Represents outgoing request for a request in coap client mode.
 *
 * Use [OutgoingRequestBuilder] for creating this object.
 */
interface OutgoingRequest : OutgoingMessage, BaseRequest {

    /**
     * Tweaks Observable behavior and allows for custom error handling.
     * If true (default), a non-2.xx response will error [CoapEndpointResponseException] the response() Observable.
     * If false, a non-2.xx response will be reported as a `next` event.
     */
    val expectSuccess: Boolean

    /**
     * Flag to treat the request as non-blockwise. In a blockwise request data is sent in chuncks(blocks)
     * of fixed size. In GG we use 1024 bytes. On the tracker some Coap resources accept larger data payloads
     * in a single block. In this case we need to force a non-blockwise request. Here is an example of this
     * type of resource:
     *
     * https://wiki.fitbit.com/pages/viewpage.action?pageId=109586801
     */
    val forceNonBlockwise: Boolean

    /**
     * Maximum number of times the client will resend the request if there is a response timeout.
     * For example, when set to 0, a request will only be sent once and not re-sent if a response
     * isn't received before the ack timeout. If maxResendCount is set to a negative value, the
     * default value [GG_COAP_DEFAULT_MAX_RETRANSMIT] from GG XP lib will be used.
     */
    val maxResendCount: Int

    /**
     * Timeout after which a resend will happen, in milliseconds.
     * Set 0 to use a default value, which is a random number within [GG_COAP_ACK_TIMEOUT_MS] sec to
     * 1.5 * [GG_COAP_ACK_TIMEOUT_MS] sec
     */
    val ackTimeout: Int
}

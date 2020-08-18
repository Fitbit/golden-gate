// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bindings.coap.data

/**
 * Implementations of this interface will be sent back to the CoAP Client in response to requests coming in to a CoAP Server
 */
interface OutgoingResponse : OutgoingMessage, BaseResponse {
    /**
     * Flag to send the response as non-blockwise. In a blockwise request data is sent in chuncks(blocks)
     * of fixed size. In GG we use 1024 bytes. For the AOSP peripheral role, some Coap resources accept
     * larger data payloads in a single block. In this case we need to force a non-blockwise response.
     */
    val forceNonBlockwise: Boolean
}

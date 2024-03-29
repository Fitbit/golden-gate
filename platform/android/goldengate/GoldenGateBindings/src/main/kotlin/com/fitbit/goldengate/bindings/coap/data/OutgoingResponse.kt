// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bindings.coap.data

/**
 * Implementations of this interface will be sent back to the CoAP Client in response to requests coming in to a CoAP Server
 */
interface OutgoingResponse : OutgoingMessage, BaseResponse {
    /**
     * Flag to use auto-configured block1 and block2 options and response code in the CoAP response
     * provided by xp lib. If handler needs to respond to non-blockwise request or reply with error
     * code, then this flag should not be set.
     *
     * If autogenerateBlockwiseConfig is set, XP lib will take care of the blockwise options
     * (BLOCK1 and BLOCK2) and the corresponding response code for you.
     */
    val autogenerateBlockwiseConfig: Boolean
}

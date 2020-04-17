// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

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

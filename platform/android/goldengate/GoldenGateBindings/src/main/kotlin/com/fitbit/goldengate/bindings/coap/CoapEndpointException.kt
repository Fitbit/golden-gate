// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bindings.coap

import com.fitbit.goldengate.bindings.coap.data.Data

/**
 * Exception indicating error when requesting data from coap server
 */
class CoapEndpointException(
    /**
     * Error message
     */
    message: String,
    /**
     * Error code
     */
    val error: Int? = null,
    /**
     * Any partial data cached so far in-memory for CoAP request before failure
     */
    val partialData: Data = Data(0),
    /**
     * stack info: DTLS state
     */
    val lastDtlsState: Int? = -1,
    /**
     * stack info: stack event
     */
    val lastStackEvent: Int? = -1,
): Exception("ErrorCode: $error, $message")

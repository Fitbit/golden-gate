// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bindings.coap

import com.fitbit.goldengate.bindings.coap.data.ExtendedError
import com.fitbit.goldengate.bindings.coap.data.ResponseCode

/**
 * Exception thrown when [ResponseCode] for response from a CoAP request is
 * [ResponseCode.Class.clientError] or [ResponseCode.Class.serverError].
 *
 * User can expect this exception when expectSuccess is true for OutgoingRequest
 */
class CoapEndpointResponseException(
    val responseCode: ResponseCode,
    val extendedError: ExtendedError?
): Exception("Response from device not OK [Response code] $responseCode")

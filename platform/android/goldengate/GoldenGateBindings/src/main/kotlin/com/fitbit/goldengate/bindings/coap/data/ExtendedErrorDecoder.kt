// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bindings.coap.data

/**
 * Decodes CoAP response body
 */
internal class ExtendedErrorDecoder {

    /**
     * Decode an extended error from a coap response body(protobuf message)
     *
     * return [ExtendedError] if message is a response error, null otherwise
     */
    fun decode(message: RawResponseMessage): ExtendedError? =
        if (message.responseCode.error()) decode(message.data) else null

    /**
     * Decode an extended error from a coap response body(protobuf message)
     *
     * @return [ExtendedError] if the given [responseBody] has valid extended error, NULL otherwise
     */
    external fun decode(responseBody: ByteArray): ExtendedError
}

// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bindings.coap.data

/**
 * Supported Coap methods. These are analogous to HTTP methods of the same names.
 *
 * @property value the byte value corresponding to this method from the CoAP Specification (RFC 7252)
 *
 * @see <a href="https://tools.ietf.org/html/rfc7252#section-5.8">CoAP Request Method Definitions</a>
 */
enum class Method(val value: Byte) {
    /**
     * Retrieve a resource. MUST not set an [OutgoingBody] that is not an instance of [EmptyOutgoingBody]
     */
    GET(1),
    /**
     * A non-idempotent request to process the data in the request body.
     */
    POST(2),
    /**
     * An idempotent request to process the data in the request body.
     */
    PUT(3),
    /**
     * Delete a resource. MUST not set an [OutgoingBody] that is not an instance of [EmptyOutgoingBody]
     */
    DELETE(4);

    companion object {

        /**
         * Create [Method] from given value
         *
         * @throws NoSuchElementException if [value] is not recognized
         */
        @JvmStatic
        fun fromValue(value: Byte): Method {
            return Method.values().first {
                it.value == value
            }
        }
    }
}

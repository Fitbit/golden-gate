// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bindings.coap.data

/**
 * Supported options that can be added to a coap message options
 *
 * <b>Note:</b> For Internal use only, use [Option] and [OutgoingRequestBuilder] for creating coap request
 */
enum class OptionNumber(val value: Short) {

    /**
     * The If-Match Option MAY be used to make a request conditional on the current existence or
     * value of an ETag for one or more representations of the target resource.
     */
    IF_MATCH(1),

    /**
     * The request MAY include one or more ETag Options, identifying responses that the client has stored.
     */
    ETAG(4),

    /**
     * The If-None-Match Option MAY be used to make a request conditional on the nonexistence of
     * the target resource.
     */
    IF_NONE_MATCH(5),

    /**
     * The values of these options specify the location at which the resource was created.
     * Otherwise, the resource was created at the request URI.
     */
    LOCATION_PATH(8),

    /**
     * Specifies one segment of the absolute path to the resource
     */
    URI_PATH(11),

    /**
     * The Content-Format Option indicates the representation format of the message payload.
     */
    CONTENT_FORMAT(12),

    /**
     * Caches can use the Max-Age Option to determine freshness and (if present) the [ETAG] Option
     * for validation
     */
    MAX_AGE(14),

    /**
     * Specifies one argument parameterizing the resource.
     */
    URI_QUERY(15),

    /**
     * Can be used to indicate which Content-Format is acceptable to the client.
     */
    ACCEPT(17),

    /**
     * The values of these options specify the location at which the resource was created.
     * Otherwise, the resource was created at the request URI.
     */
    LOCATION_QUERY(20),

    /**
     * vendor-specific option number
     */
    START_OFFSET(2048);

    companion object {

        /**
         * Create [OptionNumber] from given value
         *
         * @throws NoSuchElementException if [value] is not recognized
         */
        @JvmStatic
        fun fromValue(value: Short): OptionNumber {
            return OptionNumber.values().first {
                it.value == value
            }
        }
    }

}

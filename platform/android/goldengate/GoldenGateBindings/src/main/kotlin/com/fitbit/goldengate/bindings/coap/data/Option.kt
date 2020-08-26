// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bindings.coap.data

import com.fitbit.goldengate.bindings.coap.data.OptionNumber.ETAG
import com.fitbit.goldengate.bindings.util.toBlockInfo
import java.util.Arrays

/**
 * Individual Coap option
 */
sealed class Option(
    /** [OptionNumber] for this option */
    val number: OptionNumber,
    /** [OptionValue] for this option */
    val value: OptionValue
)

/**
 * The If-Match Option MAY be used to make a request conditional on the current existence or
 * value of an ETag for one or more representations of the target resource.
 */
data class IfMatchOption(private val opaqueValue: Opaque) : Option(OptionNumber.IF_MATCH, OpaqueOptionValue(opaqueValue)) {

    override fun equals(other: Any?): Boolean {
        if (this === other) return true
        if (javaClass != other?.javaClass) return false

        other as IfMatchOption

        if (!Arrays.equals(opaqueValue, other.opaqueValue)) return false

        return true
    }

    override fun hashCode(): Int {
        return Arrays.hashCode(opaqueValue)
    }
}

/**
 * The request MAY include one or more ETag Options, identifying responses that the client has stored.
 */
data class EtagOption(private val opaqueValue: Opaque) : Option(OptionNumber.ETAG, OpaqueOptionValue(opaqueValue)) {

    override fun equals(other: Any?): Boolean {
        if (this === other) return true
        if (javaClass != other?.javaClass) return false

        other as EtagOption

        if (!Arrays.equals(opaqueValue, other.opaqueValue)) return false

        return true
    }

    override fun hashCode(): Int {
        return Arrays.hashCode(opaqueValue)
    }
}

/**
 * The If-None-Match Option MAY be used to make a request conditional on the nonexistence of
 * the target resource.
 *
 * <b>Note:</b> This is singleton instead of data class as it does not hold any data
 */
object IfNoneMatchOption : Option(OptionNumber.IF_NONE_MATCH, EmptyOptionValue())

/**
 * The values of these options specify the location at which the resource was created.
 * Otherwise, the resource was created at the request URI.
 */
data class LocationPathOption(private val stringValue: String) : Option(OptionNumber.LOCATION_PATH, StringOptionValue(stringValue))

/**
 * The Content-Format Option indicates the representation format of the message payload.
 */
data class ContentFormatOption(private val formatValue: FormatOptionValue) : Option(OptionNumber.CONTENT_FORMAT, formatValue.intValue)

/**
 * Caches can use the Max-Age Option to determine freshness and (if present) the [ETAG] Option
 * for validation
 */
data class MaxAgeOption(private val intValue: Int) : Option(OptionNumber.MAX_AGE, IntOptionValue(intValue))

/**
 * Specifies one argument parameterizing the resource.
 */
data class UriQueryOption(private val stringValue: String) : Option(OptionNumber.URI_QUERY, StringOptionValue(stringValue))

/**
 * Can be used to indicate which Content-Format is acceptable to the client.
 */
data class AcceptOption(private val formatValue: FormatOptionValue) : Option(OptionNumber.ACCEPT, formatValue.intValue)

/**
 * The values of these options specify the location at which the resource was created.
 * Otherwise, the resource was created at the request URI.
 */
data class LocationQueryOption(private val stringValue: String) : Option(OptionNumber.URI_QUERY, StringOptionValue(stringValue))

/**
 * Vendor(Fitbit) option for start offset in a file
 */
data class StartOffset(private val intValue: Int): Option(OptionNumber.START_OFFSET, IntOptionValue(intValue))

/**
 * Specifies one segment of the absolute path to the resource
 *
 * <b>Note:</b> New Coap Request should use [OutgoingRequestBuilder] for supplying uri path
 */
internal data class UriPathOption(private val stringValue: String) : Option(OptionNumber.URI_PATH, StringOptionValue(stringValue))

/**
 * This option is used to indicate the block transfer info for BLOCK1.
 */
data class Block1Option(private val intValue: Int) : Option(OptionNumber.BLOCK1, BlockOptionValue(intValue.toBlockInfo()))

/**
 * This option is used to indicate the block transfer info for BLOCK2.
 */
data class Block2Option(private val intValue: Int) : Option(OptionNumber.BLOCK2, BlockOptionValue(intValue.toBlockInfo()))

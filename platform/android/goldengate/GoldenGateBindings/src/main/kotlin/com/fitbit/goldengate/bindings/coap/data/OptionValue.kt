// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bindings.coap.data

/**
 * Supported value types for a coap option
 */
sealed class OptionValue

/**
 * For option that does not have a value, like [OptionNumber.IF_NONE_MATCH]
 */
class EmptyOptionValue : OptionValue()

/**
 * Option value with [Int] type
 */
data class IntOptionValue(val value: Int) : OptionValue()

/**
 * Option value with [String] type
 */
data class StringOptionValue(val value: String) : OptionValue()

/**
 * Option value with [Opaque] type
 */
class OpaqueOptionValue(val value: Opaque) : OptionValue()

/**
 * Option value with [BlockInfo] type
 */
data class BlockOptionValue(val value: BlockInfo) : OptionValue()

/**
 * The Content-Format Option indicates the representation format of the message payload.
 * These are the supported value for [ContentFormatOption] and [AcceptOption] coap options
 */
enum class FormatOptionValue(val intValue: IntOptionValue) {
    TEXT_PLAIN(IntOptionValue(0)),
    LINK_FORMAT(IntOptionValue(40)),
    XML(IntOptionValue(41)),
    OCTET_STREAM(IntOptionValue(42)),
    EXI(IntOptionValue(47)),
    JSON(IntOptionValue(50)),
    CBOR(IntOptionValue(60));

    companion object {
        /**
         * Create [FormatOptionValue] from given value
         *
         * @throws NoSuchElementException if [value] is not recognized
         */
        fun fromValue(value: Int): FormatOptionValue {
            return FormatOptionValue.values().first {
                it.intValue.value == value
            }
        }
    }
}

package com.fitbit.goldengate.bindings.coap.data

import com.fitbit.goldengate.bindings.coap.data.OptionNumber.ACCEPT
import com.fitbit.goldengate.bindings.coap.data.OptionNumber.CONTENT_FORMAT
import com.fitbit.goldengate.bindings.coap.data.OptionNumber.ETAG
import com.fitbit.goldengate.bindings.coap.data.OptionNumber.IF_MATCH
import com.fitbit.goldengate.bindings.coap.data.OptionNumber.IF_NONE_MATCH
import com.fitbit.goldengate.bindings.coap.data.OptionNumber.LOCATION_PATH
import com.fitbit.goldengate.bindings.coap.data.OptionNumber.LOCATION_QUERY
import com.fitbit.goldengate.bindings.coap.data.OptionNumber.MAX_AGE
import com.fitbit.goldengate.bindings.coap.data.OptionNumber.START_OFFSET
import com.fitbit.goldengate.bindings.coap.data.OptionNumber.URI_PATH
import com.fitbit.goldengate.bindings.coap.data.OptionNumber.URI_QUERY
import timber.log.Timber

/**
 * Builder to build coap options, for internal use only and called from JNI
 */
internal class OptionsBuilder {

    private val options = Options()

    /**
     * Add option with empty value
     */
    fun option(optionNumberValue: Short) =
            optionInternal(optionNumberValue, null)

    /**
     * Add option with int value
     */
    fun option(optionNumberValue: Short, optionValue: Int) =
            optionInternal(optionNumberValue, optionValue)

    /**
     * Add option with string value
     */
    fun option(optionNumberValue: Short, optionValue: String) =
            optionInternal(optionNumberValue, optionValue)

    /**
     * Add option with opaque value
     */
    fun option(optionNumberValue: Short, optionValue: Opaque) =
            optionInternal(optionNumberValue, optionValue)

    private fun optionInternal(optionNumberValue: Short, optionValue: Any?) {

        try {
            val optionNumber = OptionNumber.fromValue(optionNumberValue)
            val option = when (optionNumber) {
                IF_MATCH -> IfMatchOption(optionValue as Opaque)
                ETAG -> EtagOption(optionValue as ByteArray)
                IF_NONE_MATCH -> IfNoneMatchOption
                LOCATION_PATH -> LocationPathOption(optionValue as String)
                URI_PATH -> UriPathOption(optionValue as String)
                CONTENT_FORMAT -> ContentFormatOption(FormatOptionValue.fromValue(optionValue as Int))
                MAX_AGE -> MaxAgeOption(optionValue as Int)
                URI_QUERY -> UriQueryOption(optionValue as String)
                ACCEPT -> AcceptOption(FormatOptionValue.fromValue(optionValue as Int))
                LOCATION_QUERY -> LocationQueryOption(optionValue as String)
                START_OFFSET -> StartOffset(optionValue as Int)
            }
            options.add(option)
        } catch (ex: NoSuchElementException) {
            // ignoring to support for any option added to Node that is not supported in this Hub bindings version
            Timber.d("Skipping adding option: $optionNumberValue with value: $optionValue as its not supported")
        }
    }

    fun build(): Options {
        return options
    }

}
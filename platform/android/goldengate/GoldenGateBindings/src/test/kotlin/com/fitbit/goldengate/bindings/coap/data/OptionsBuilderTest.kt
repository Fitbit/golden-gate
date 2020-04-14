package com.fitbit.goldengate.bindings.coap.data

import org.junit.Assert.*
import org.junit.Test

class OptionsBuilderTest {

    @Test
    fun shouldNotAddOptionNumberThatIsNotSupported() {
        /** 996 format is not supported by [OptionNumber] */
        val builder = OptionsBuilder()
        builder.option(996, 5)
        val options = builder.build()
        assertEquals(0, options.size)
    }

    @Test
    fun shouldNotAddFormatOptionValueThatIsNotSupported() {
        /** 995 format is not supported by [FormatOptionValue] */
        val builder = OptionsBuilder()
        builder.option(OptionNumber.CONTENT_FORMAT.value, 995)
        val options = builder.build()
        assertEquals(0, options.size)
    }

    @Test
    fun shouldAddOptionWithEmptyValue() {
        val builder = OptionsBuilder()
        builder.option(OptionNumber.IF_NONE_MATCH.value)
        val options = builder.build()
        assertEquals(1, options.size)
        assertEquals(OptionNumber.IF_NONE_MATCH, options[0].number)
    }

    @Test
    fun shouldAddOptionWithIntValue() {
        val builder = OptionsBuilder()
        builder.option(OptionNumber.MAX_AGE.value, 1)
        val options = builder.build()
        assertEquals(1, options.size)
        assertEquals(OptionNumber.MAX_AGE, options[0].number)
        assertTrue(options[0].value is IntOptionValue)
        assertEquals(1, (options[0].value as IntOptionValue).value)
    }

    @Test
    fun shouldAddOptionWithStringValue() {
        val builder = OptionsBuilder()
        builder.option(OptionNumber.LOCATION_PATH.value, "value_1")
        val options = builder.build()
        assertEquals(1, options.size)
        assertEquals(OptionNumber.LOCATION_PATH, options[0].number)
        assertTrue(options[0].value is StringOptionValue)
        assertEquals("value_1", (options[0].value as StringOptionValue).value)
    }

    @Test
    fun shouldAddOptionWithOpaqueValue() {
        val builder = OptionsBuilder()
        builder.option(OptionNumber.ETAG.value, "value_1".toByteArray())
        val options = builder.build()
        assertEquals(1, options.size)
        assertEquals(OptionNumber.ETAG, options[0].number)
        assertTrue(options[0].value is OpaqueOptionValue)
        assertEquals("value_1", String((options[0].value as OpaqueOptionValue).value))
    }
}


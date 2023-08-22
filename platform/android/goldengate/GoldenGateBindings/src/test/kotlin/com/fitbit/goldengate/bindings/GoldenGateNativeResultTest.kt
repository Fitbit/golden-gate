package com.fitbit.goldengate.bindings

import org.junit.Assert.*
import org.junit.Test

class GoldenGateNativeResultTest {

    @Test
    fun `should convert error code to GoldenGateNativeResult enum constant`() {
        assertEquals(GoldenGateNativeResult.getNativeResultFrom(0), GoldenGateNativeResult.GG_SUCCESS)
        assertEquals(GoldenGateNativeResult.getNativeResultFrom(100), GoldenGateNativeResult.GG_SUCCESS)
        assertEquals(GoldenGateNativeResult.getNativeResultFrom(-1), GoldenGateNativeResult.GG_FAILURE)
        assertEquals(GoldenGateNativeResult.getNativeResultFrom(-100), GoldenGateNativeResult.GG_FAILURE)

        assertEquals(GoldenGateNativeResult.getNativeResultFrom(-10000), GoldenGateNativeResult.GG_ERROR_OUT_OF_MEMORY)
        assertEquals(GoldenGateNativeResult.getNativeResultFrom(-10013), GoldenGateNativeResult.GG_ERROR_OVERFLOW)
        assertEquals(GoldenGateNativeResult.getNativeResultFrom(-10018), GoldenGateNativeResult.GG_ERROR_IN_USE)
        assertEquals(GoldenGateNativeResult.getNativeResultFrom(-10099), GoldenGateNativeResult.GG_FAILURE)

        assertEquals(GoldenGateNativeResult.getNativeResultFrom(-10100), GoldenGateNativeResult.GG_ERROR_EOS)
        assertEquals(GoldenGateNativeResult.getNativeResultFrom(-10199), GoldenGateNativeResult.GG_FAILURE)

        assertEquals(GoldenGateNativeResult.getNativeResultFrom(-10200), GoldenGateNativeResult.GG_ERROR_CONNECTION_RESET)
        assertEquals(GoldenGateNativeResult.getNativeResultFrom(-10217), GoldenGateNativeResult.GG_ERROR_UNMAPPED_STACK_ERROR)
        assertEquals(GoldenGateNativeResult.getNativeResultFrom(-10299), GoldenGateNativeResult.GG_FAILURE)

        assertEquals(GoldenGateNativeResult.getNativeResultFrom(-10300), GoldenGateNativeResult.GG_ERROR_COAP_UNSUPPORTED_VERSION)
        assertEquals(GoldenGateNativeResult.getNativeResultFrom(-10306), GoldenGateNativeResult.GG_ERROR_COAP_ETAG_MISMATCH)
        assertEquals(GoldenGateNativeResult.getNativeResultFrom(-10399), GoldenGateNativeResult.GG_FAILURE)

        assertEquals(GoldenGateNativeResult.getNativeResultFrom(-10400), GoldenGateNativeResult.GG_ERROR_REMOTE_EXIT)
        assertEquals(GoldenGateNativeResult.getNativeResultFrom(-10499), GoldenGateNativeResult.GG_FAILURE)

        assertEquals(GoldenGateNativeResult.getNativeResultFrom(-10500), GoldenGateNativeResult.GG_ERROR_GATTLINK_UNEXPECTED_PSN)
        assertEquals(GoldenGateNativeResult.getNativeResultFrom(-10599), GoldenGateNativeResult.GG_FAILURE)

        assertEquals(GoldenGateNativeResult.getNativeResultFrom(-10600), GoldenGateNativeResult.GG_ERROR_TLS_FATAL_ALERT_MESSAGE)
        assertEquals(GoldenGateNativeResult.getNativeResultFrom(-10604), GoldenGateNativeResult.GG_ERROR_TLS_UNMAPPED_LIB_ERROR)
        assertEquals(GoldenGateNativeResult.getNativeResultFrom(-10699), GoldenGateNativeResult.GG_FAILURE)

        assertEquals(GoldenGateNativeResult.getNativeResultFrom(-10700), GoldenGateNativeResult.GG_FAILURE)

        assertEquals(GoldenGateNativeResult.getNativeResultFrom(-12000), GoldenGateNativeResult.GG_ERROR_ERRNO)
        assertEquals(GoldenGateNativeResult.getNativeResultFrom(-12999), GoldenGateNativeResult.GG_ERROR_ERRNO)

        assertEquals(GoldenGateNativeResult.getNativeResultFrom(Int.MAX_VALUE), GoldenGateNativeResult.GG_SUCCESS)
        assertEquals(GoldenGateNativeResult.getNativeResultFrom(Int.MIN_VALUE), GoldenGateNativeResult.GG_FAILURE)
    }
}

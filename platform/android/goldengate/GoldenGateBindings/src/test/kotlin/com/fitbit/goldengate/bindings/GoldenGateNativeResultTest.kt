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

        assertEquals(GoldenGateNativeResult.getNativeResultFrom(-10100), GoldenGateNativeResult.GG_ERROR_BASE_IO)
        assertEquals(GoldenGateNativeResult.getNativeResultFrom(-10199), GoldenGateNativeResult.GG_ERROR_BASE_IO)

        assertEquals(GoldenGateNativeResult.getNativeResultFrom(-10200), GoldenGateNativeResult.GG_ERROR_BASE_SOCKET)
        assertEquals(GoldenGateNativeResult.getNativeResultFrom(-10299), GoldenGateNativeResult.GG_ERROR_BASE_SOCKET)

        assertEquals(GoldenGateNativeResult.getNativeResultFrom(-10300), GoldenGateNativeResult.GG_ERROR_BASE_COAP)
        assertEquals(GoldenGateNativeResult.getNativeResultFrom(-10399), GoldenGateNativeResult.GG_ERROR_BASE_COAP)

        assertEquals(GoldenGateNativeResult.getNativeResultFrom(-10400), GoldenGateNativeResult.GG_ERROR_BASE_REMOTE)
        assertEquals(GoldenGateNativeResult.getNativeResultFrom(-10499), GoldenGateNativeResult.GG_ERROR_BASE_REMOTE)

        assertEquals(GoldenGateNativeResult.getNativeResultFrom(-10500), GoldenGateNativeResult.GG_ERROR_BASE_GATTLINK)
        assertEquals(GoldenGateNativeResult.getNativeResultFrom(-10599), GoldenGateNativeResult.GG_ERROR_BASE_GATTLINK)

        assertEquals(GoldenGateNativeResult.getNativeResultFrom(-10600), GoldenGateNativeResult.GG_ERROR_BASE_TLS)
        assertEquals(GoldenGateNativeResult.getNativeResultFrom(-10699), GoldenGateNativeResult.GG_ERROR_BASE_TLS)

        assertEquals(GoldenGateNativeResult.getNativeResultFrom(-10700), GoldenGateNativeResult.GG_FAILURE)

        assertEquals(GoldenGateNativeResult.getNativeResultFrom(Int.MAX_VALUE), GoldenGateNativeResult.GG_SUCCESS)
        assertEquals(GoldenGateNativeResult.getNativeResultFrom(Int.MIN_VALUE), GoldenGateNativeResult.GG_FAILURE)
    }
}

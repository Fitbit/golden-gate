package com.fitbit.goldengate.bindings.util

import org.junit.Test
import kotlin.test.assertFalse
import kotlin.test.assertTrue

class NativeReferenceExtKtTest {
    @Test
    fun `should return true when it is a non-zero value`() {
        val nativeRefAddress1: Long = -755482576
        val nativeRefAddress2: Long = 517036029296
        val nativeRefAddress3: Long = 0

        assertTrue { nativeRefAddress1.isNotNull() }
        assertTrue { nativeRefAddress2.isNotNull() }
        assertFalse { nativeRefAddress3.isNotNull() }
    }
}

// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bindings.dtls

import org.mockito.kotlin.mock
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Test

class HelloTlsKeyResolverTest {

    private val resolver = HelloTlsKeyResolver()

    @Test
    fun shouldReturnKeyForValidKeyId() {
        assertNotNull(resolver.resolve(mock(), HELLO_KEY_ID))
    }

    @Test
    fun shouldNullForInvalidKeyId() {
        assertNull(resolver.resolve(mock(), "unknown_key".toByteArray()))
    }

}

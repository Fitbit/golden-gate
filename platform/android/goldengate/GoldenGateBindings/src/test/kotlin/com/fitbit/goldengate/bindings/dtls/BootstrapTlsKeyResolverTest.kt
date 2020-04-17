// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bindings.dtls

import com.nhaarman.mockitokotlin2.mock
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Test

class BootstrapTlsKeyResolverTest {

    private val resolver = BootstrapTlsKeyResolver()

    @Test
    fun shouldReturnKeyForValidKeyId() {
        assertNotNull(resolver.resolve(mock(), BOOTSTRAP_KEY_ID))
    }

    @Test
    fun shouldNullForInvalidKeyId() {
        assertNull(resolver.resolve(mock(), "unknown_key".toByteArray()))
    }
}

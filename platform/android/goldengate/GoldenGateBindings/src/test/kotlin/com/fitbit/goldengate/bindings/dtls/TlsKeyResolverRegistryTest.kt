// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bindings.dtls

import org.mockito.kotlin.mock
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Test

class TlsKeyResolverRegistryTest {

    @Test
    fun shouldReturnKeyForValidKeyId() {
        assertNotNull(TlsKeyResolverRegistry.resolvers.resolve(mock(), HELLO_KEY_ID))
        assertNotNull(TlsKeyResolverRegistry.resolvers.resolve(mock(), BOOTSTRAP_KEY_ID))
    }

    @Test
    fun shouldNullForInvalidKeyId() {
        assertNull(TlsKeyResolverRegistry.resolvers.resolve(mock(), "unknown_key".toByteArray()))
    }
}

// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.remoteapi

import com.fitbit.remoteapi.handlers.ConfigurationRpc
import com.nhaarman.mockitokotlin2.mock
import org.junit.Test
import java.util.Collections

class ConfigurationRpcHandlerTest {

    @Test(expected = IllegalArgumentException::class)
    fun testThrowsOnNullParams() {
        ConfigurationRpc.Parser(mock(), mock()).parse(Collections.emptyList())
    }
}

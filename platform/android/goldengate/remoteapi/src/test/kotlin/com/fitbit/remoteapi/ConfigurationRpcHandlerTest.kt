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

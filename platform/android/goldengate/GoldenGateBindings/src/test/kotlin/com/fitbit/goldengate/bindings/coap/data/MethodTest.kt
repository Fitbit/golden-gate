package com.fitbit.goldengate.bindings.coap.data

import org.junit.Assert.assertEquals
import org.junit.Test

class MethodTest {

    @Test
    fun shouldReturnMethodForValidValues() {
        assertEquals(Method.GET, Method.fromValue(1))
        assertEquals(Method.POST, Method.fromValue(2))
        assertEquals(Method.PUT, Method.fromValue(3))
        assertEquals(Method.DELETE, Method.fromValue(4))
    }

    @Test(expected = NoSuchElementException::class)
    fun shouldThrowExceptionForInvalidValues() {
        Method.fromValue(5)
    }
}

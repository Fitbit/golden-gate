// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bindings.coap.block

import com.fitbit.goldengate.bindings.coap.data.BytesArrayOutgoingBody
import com.fitbit.goldengate.bindings.coap.data.EmptyOutgoingBody
import com.fitbit.goldengate.bindings.coap.data.Method
import com.fitbit.goldengate.bindings.coap.data.OutgoingBody
import com.fitbit.goldengate.bindings.coap.data.OutgoingRequest
import com.nhaarman.mockitokotlin2.mock
import com.nhaarman.mockitokotlin2.whenever
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Test

class BlockDataSourceCreatorTest {

    @Test(expected = IllegalArgumentException::class)
    fun shouldThrowExceptionForGetRequestWithNonEmptyBody() {
        createBlockDataSourceWithNonEmptyOutgoingBody(Method.GET)
    }

    @Test(expected = IllegalArgumentException::class)
    fun shouldThrowExceptionForDeleteRequestWithNonEmptyBody() {
        createBlockDataSourceWithNonEmptyOutgoingBody(Method.DELETE)
    }

    @Test
    fun shouldReturnNullSourceForGetRequest() {
        val source = createBlockDataSourceWithEmptyOutgoingBody(Method.GET)
        assertNull(source)
    }

    @Test
    fun shouldReturnNullSourceForDeleteRequest() {
        val source = createBlockDataSourceWithEmptyOutgoingBody(Method.DELETE)
        assertNull(source)
    }

    @Test
    fun shouldReturnNotNullSourceForPostRequest() {
        val source = createBlockDataSourceWithEmptyOutgoingBody(Method.POST)
        assertNotNull(source)
    }

    @Test
    fun shouldReturnNotNullSourceForPutRequest() {
        val source = createBlockDataSourceWithEmptyOutgoingBody(Method.PUT)
        assertNotNull(source)
    }



    private fun createBlockDataSourceWithEmptyOutgoingBody(method: Method) =
            createBlockDataSourceWithOutgoingBody(method, EmptyOutgoingBody())

    private fun createBlockDataSourceWithNonEmptyOutgoingBody(method: Method) =
            createBlockDataSourceWithOutgoingBody(method, BytesArrayOutgoingBody("hello".toByteArray()))

    private fun createBlockDataSourceWithOutgoingBody(method: Method, body: OutgoingBody): BlockDataSource? {
        val mockRequest = mock<OutgoingRequest>()
        whenever(mockRequest.method).thenReturn(method)
        whenever(mockRequest.body).thenReturn(body)
        whenever(mockRequest.progressObserver).thenReturn(mock())

        return BlockDataSourceCreator().create(mockRequest)
    }

}

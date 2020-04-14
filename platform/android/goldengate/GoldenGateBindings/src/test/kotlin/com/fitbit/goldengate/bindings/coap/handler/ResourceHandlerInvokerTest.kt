package com.fitbit.goldengate.bindings.coap.handler

import com.fitbit.goldengate.bindings.coap.data.Method
import com.fitbit.goldengate.bindings.coap.data.OutgoingResponse
import com.fitbit.goldengate.bindings.coap.data.RawRequestMessage
import com.nhaarman.mockitokotlin2.any
import com.nhaarman.mockitokotlin2.mock
import com.nhaarman.mockitokotlin2.verify
import com.nhaarman.mockitokotlin2.whenever
import io.reactivex.Single
import org.junit.Before
import org.junit.Test

internal class ResourceHandlerInvokerTest {

    private val mockResourceHandler = mock<ResourceHandler>()
    private val mockRawRequestMessage = mock<RawRequestMessage>()
    private val mockResponse = mock<OutgoingResponse>()

    private val invoker = ResourceHandlerInvoker(mockResourceHandler)

    @Before
    fun setup() {
        whenever(mockResourceHandler.onGet(any(), any())).thenReturn(Single.just(mockResponse))
        whenever(mockResourceHandler.onPost(any(), any())).thenReturn(Single.just(mockResponse))
        whenever(mockResourceHandler.onPut(any(), any())).thenReturn(Single.just(mockResponse))
        whenever(mockResourceHandler.onDelete(any(), any())).thenReturn(Single.just(mockResponse))
    }

    @Test
    fun shouldDelegateToGetHandler() {
        whenever(mockRawRequestMessage.method).thenReturn(Method.GET)

        invoker.invoke(mockRawRequestMessage)

        verify(mockResourceHandler).onGet(any(), any())
    }

    @Test
    fun shouldDelegateToPostHandler() {
        whenever(mockRawRequestMessage.method).thenReturn(Method.POST)

        invoker.invoke(mockRawRequestMessage)

        verify(mockResourceHandler).onPost(any(), any())
    }

    @Test
    fun shouldDelegateToPutHandler() {
        whenever(mockRawRequestMessage.method).thenReturn(Method.PUT)

        invoker.invoke(mockRawRequestMessage)

        verify(mockResourceHandler).onPut(any(), any())
    }

    @Test
    fun shouldDelegateToDeleteHandler() {
        whenever(mockRawRequestMessage.method).thenReturn(Method.DELETE)

        invoker.invoke(mockRawRequestMessage)

        verify(mockResourceHandler).onDelete(any(), any())
    }
}

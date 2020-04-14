package com.fitbit.goldengate.bindings.coap.data

import com.nhaarman.mockitokotlin2.mock
import io.reactivex.Observer
import org.junit.Assert
import org.junit.Assert.assertEquals
import org.junit.Test
import java.util.Arrays

class OutgoingRequestBuilderTest {

    @Test
    fun shouldBuildEmptyPath() {
        val request = OutgoingRequestBuilder("", Method.GET).build()

        assertEquals(0, getPath(request.options).size)
    }

    @Test
    fun shouldBuildWithDelimiter() {
        val request = OutgoingRequestBuilder("/", Method.GET).build()

        assertEquals(0, getPath(request.options).size)
    }

    @Test
    fun shouldBuildSingleLevelPath() {
        val request = OutgoingRequestBuilder("foo", Method.GET).build()

        assertEquals(1, getPath(request.options).size)
        assertEquals("foo", getPath(request.options, 0))
    }

    @Test
    fun shouldBuildWithTwoLevelPath() {
        val request = OutgoingRequestBuilder("foo/bar", Method.GET).build()

        assertEquals(2, getPath(request.options).size)
        assertEquals("foo", getPath(request.options, 0))
        assertEquals("bar", getPath(request.options, 1))
    }

    @Test
    fun shouldBuildWithDelimiterAtStart() {
        val request = OutgoingRequestBuilder("/foo/bar", Method.GET).build()

        assertEquals(2, getPath(request.options).size)
        assertEquals("foo", getPath(request.options, 0))
        assertEquals("bar", getPath(request.options, 1))
    }

    @Test
    fun shouldBuildWithDelimiterAtEnd() {
        val request = OutgoingRequestBuilder("foo/bar/", Method.GET).build()

        assertEquals(2, getPath(request.options).size)
        assertEquals("foo", getPath(request.options, 0))
        assertEquals("bar", getPath(request.options, 1))
    }

    @Test
    fun shouldBuildWithDelimiterAtStartAndEnd() {
        val request = OutgoingRequestBuilder("/foo/bar/", Method.GET).build()

        assertEquals(2, getPath(request.options).size)
        assertEquals("foo", getPath(request.options, 0))
        assertEquals("bar", getPath(request.options, 1))
    }

    @Test
    fun shouldBuildRequestWithoutBody() {
        val request = OutgoingRequestBuilder("foo", Method.GET).build()

        assertEquals(Method.GET, request.method)
        assertEquals(false, request.expectSuccess)
    }

    @Test
    fun shouldBuildRequestWithBodyByteArray() {
        val testByteArray = "testData".toByteArray()
        val request = OutgoingRequestBuilder("foo", Method.GET).body(testByteArray).build()

        Assert.assertTrue(Arrays.equals(testByteArray, (request.body as BytesArrayOutgoingBody).data))
    }
    @Test
    fun shouldBuildRequestWithBodyAndObserver() {
        val testByteArray = "testData".toByteArray()
        val observer:Observer<Int> = mock()
        val request = OutgoingRequestBuilder("foo", Method.GET)
            .progressObserver(observer)
            .body(testByteArray)
            .build()

        Assert.assertTrue(Arrays.equals(testByteArray, (request.body as BytesArrayOutgoingBody).data))
        Assert.assertEquals(observer, request.progressObserver)
    }

    @Test
    fun shouldBuildRequestWithBodyInputStream() {
        val testByteArray = "testData".toByteArray()
        val testInputStream = testByteArray.inputStream()
        val request = OutgoingRequestBuilder("foo", Method.GET).body(testInputStream).build()

        Assert.assertTrue(Arrays.equals(testByteArray, (request.body as InputStreamOutgoingBody).data.readBytes()))
    }

    private fun getPath(options: Options, idx: Int): String {
        return (getPath(options)[idx].value as StringOptionValue).value
    }

    private fun getPath(options: Options): List<Option> {
        return options.filter { option ->
            option.number == OptionNumber.URI_PATH
        }
    }

}

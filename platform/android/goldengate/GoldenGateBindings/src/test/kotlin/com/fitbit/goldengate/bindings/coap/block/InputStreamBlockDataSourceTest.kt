// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bindings.coap.block

import com.nhaarman.mockitokotlin2.mock
import com.nhaarman.mockitokotlin2.verify
import io.reactivex.Observer
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import java.util.Arrays

class InputStreamBlockDataSourceTest {

    @Test
    fun shouldReturnZeroBlockSizeIfSizeIsNegative() {
        val dataStream = "hello".toByteArray().inputStream()
        val dataSource = InputStreamBlockDataSource(dataStream)

        val blockSize = dataSource.getDataSize(5, -4)

        assertEquals(0, blockSize.size)
        assertFalse(blockSize.more)
        assertFalse(blockSize.requestInRange)
    }

    @Test
    fun shouldReturnHasMoreIfRequestedSizeIsInsideDataRange() {
        val dataStream = "hello".toByteArray().inputStream()
        val dataSource = InputStreamBlockDataSource(dataStream)

        val blockSize = dataSource.getDataSize(0, 1)

        assertEquals(1, blockSize.size)
        assertTrue(blockSize.more)
        assertTrue(blockSize.requestInRange)
    }

    @Test
    fun shouldReturnHasNoMoreIfRequestedSizeIsAtDataRange() {
        val dataStream = "hello".toByteArray().inputStream()
        val dataSource = InputStreamBlockDataSource(dataStream)

        val blockSize = dataSource.getDataSize(0, 5)

        assertEquals(5, blockSize.size)
        assertFalse(blockSize.more)
        assertTrue(blockSize.requestInRange)
    }

    @Test(expected = IllegalArgumentException::class)
    fun shouldThrowExceptionIfOffsetIsNegative() {
        val dataStream = "hello".toByteArray().inputStream()
        val dataSource = InputStreamBlockDataSource(dataStream)

        dataSource.getData(-5, 1)
    }

    @Test(expected = IllegalArgumentException::class)
    fun shouldThrowExceptionIfRequestedRangeGreaterThanDataSize() {
        val dataStream = "hello".toByteArray().inputStream()
        val dataSource = InputStreamBlockDataSource(dataStream)

        dataSource.getData(0, 20)
    }

    @Test
    fun shouldReturnRequestedBytesIfInLowerRange() {
        val dataStream = "hello".toByteArray().inputStream()
        val dataSource = InputStreamBlockDataSource(dataStream)

        val dataBytes = dataSource.getData(0, 3)

        assertTrue(Arrays.equals("hel".toByteArray(), dataBytes))
    }

    @Test
    fun shouldReturnRequestedBytesIfInMiddleRange() {
        val dataStream = "hello".toByteArray().inputStream()
        val dataSource = InputStreamBlockDataSource(dataStream)
        //skip first 2 bytes
        dataSource.getData(0,2)
        val dataBytes = dataSource.getData(2, 2)

        assertTrue(Arrays.equals("ll".toByteArray(), dataBytes))
    }

    @Test
    fun shouldReturnRequestedBytesIfInHigherRange() {
        val dataStream = "hello".toByteArray().inputStream()
        val dataSource = InputStreamBlockDataSource(dataStream)
        //skip first 2 bytes
        dataSource.getData(0,2)
        val dataBytes = dataSource.getData(2, 3)

        assertTrue(Arrays.equals("llo".toByteArray(), dataBytes))
    }

    @Test
    fun shouldReturnRequestedBytesIfAtMaxRange() {
        val dataStream = "hello".toByteArray().inputStream()
        val dataSource = InputStreamBlockDataSource(dataStream)

        val dataBytes = dataSource.getData(0, 5)

        assertTrue(Arrays.equals("hello".toByteArray(), dataBytes))
    }

    @Test
    fun shouldNotifyProgressWhenGettingData() {
        val observer = mock<Observer<Int>>()
        val dataStream = "hello".toByteArray().inputStream()
        val dataSource = InputStreamBlockDataSource(dataStream, observer)

        dataSource.getData(0, 2)
        dataSource.getData(2, 3)
        verify(observer).onNext(0)
        verify(observer).onNext(2)
    }
}

package com.fitbit.goldengate.bindings.coap.block

import org.mockito.kotlin.mock
import org.mockito.kotlin.verify
import io.reactivex.Observer
import org.junit.Assert
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import java.io.File

class FileUriBlockDataSourceTest {

    @Rule @JvmField
    val testFolder = TemporaryFolder()
    private lateinit var tempFile: File

    @Before
    fun setup() {
        tempFile = testFolder.newFile("data")
        tempFile.writeBytes(byteArrayOf(0,1,2,3,4,5,6,7,8,9))
    }

    @Test
    fun shouldReturnZeroBlockSizeIfOffsetIsGreaterThanDataSize() {
        val dataSource = FileUriBlockDataSource(tempFile.toURI())
        val blockSize = dataSource.getDataSize(100, 5)

        Assert.assertEquals(0, blockSize.size)
        Assert.assertFalse(blockSize.more)
        Assert.assertFalse(blockSize.requestInRange)
    }

    @Test
    fun shouldReturnHasMoreIfRequestedSizeIsWithinDataRange() {
        val dataSource = FileUriBlockDataSource(tempFile.toURI())
        val blockSize = dataSource.getDataSize(1, 5)

        Assert.assertEquals(5, blockSize.size)
        Assert.assertTrue(blockSize.more)
        Assert.assertTrue(blockSize.requestInRange)
    }

    @Test
    fun shouldReturnHasNoMoreIfRequestedsizeIsAtDataRange() {
        val dataSource = FileUriBlockDataSource(tempFile.toURI())
        val blockSize = dataSource.getDataSize(5, 5)

        Assert.assertEquals(5, blockSize.size)
        Assert.assertFalse(blockSize.more)
        Assert.assertTrue(blockSize.requestInRange)
    }

    @Test
    fun shouldReturnZeroBlockSizeIfFileCannotBeOpened() {
        tempFile.delete()

        val dataSource = FileUriBlockDataSource(tempFile.toURI())
        val blockSize = dataSource.getDataSize(1, 5)

        Assert.assertEquals(0, blockSize.size)
        Assert.assertFalse(blockSize.more)
        Assert.assertFalse(blockSize.requestInRange)
    }

    @Test
    fun shouldReturnZeroBlockSizeIfOffsetIsNegative() {
        val dataSource = FileUriBlockDataSource(tempFile.toURI())
        val blockSize = dataSource.getDataSize(-5, 5)

        Assert.assertEquals(0, blockSize.size)
        Assert.assertFalse(blockSize.more)
        Assert.assertFalse(blockSize.requestInRange)
    }

    @Test(expected = IllegalArgumentException::class)
    fun shouldThrowExceptionIfRequestedRangeGreaterThanDataSize() {
        val dataSource = FileUriBlockDataSource(tempFile.toURI())

        dataSource.getData(0, 20)
    }

    @Test
    fun shouldReturnRequestedBytesIfInLowerRange() {
        val dataSource = FileUriBlockDataSource(tempFile.toURI())
        val dataBytes = dataSource.getData(0, 3)

        Assert.assertTrue(byteArrayOf(0, 1, 2).contentEquals(dataBytes))
    }

    @Test
    fun shouldReturnRequestedBytesIfInMiddleRange() {
        val dataSource = FileUriBlockDataSource(tempFile.toURI())
        val dataBytes = dataSource.getData(2, 2)

        Assert.assertTrue(byteArrayOf(2, 3).contentEquals(dataBytes))
    }

    @Test
    fun shouldReturnRequestedBytesIfInHigherRange() {
        val dataSource = FileUriBlockDataSource(tempFile.toURI())
        val dataBytes = dataSource.getData(7, 3)

        Assert.assertTrue(byteArrayOf(7, 8, 9).contentEquals(dataBytes))
    }

    @Test
    fun shouldReturnRequestedBytesIfAtMaxRange() {
        val dataSource = FileUriBlockDataSource(tempFile.toURI())
        val dataBytes = dataSource.getData(0, 10)

        Assert.assertTrue(byteArrayOf(0,1,2,3,4,5,6,7,8,9).contentEquals(dataBytes))
    }

    @Test
    fun shouldNotifyProgressWhenGettingData() {
        val observer = mock<Observer<Int>>()
        val dataSource = FileUriBlockDataSource(tempFile.toURI(), observer)

        dataSource.getData(0, 2)
        dataSource.getData(2, 3)
        verify(observer).onNext(0)
        verify(observer).onNext(2)
    }
}

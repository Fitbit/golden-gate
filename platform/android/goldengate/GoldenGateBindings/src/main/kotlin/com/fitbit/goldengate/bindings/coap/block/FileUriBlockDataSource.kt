package com.fitbit.goldengate.bindings.coap.block

import com.fitbit.goldengate.bindings.coap.data.Data
import io.reactivex.Observer
import timber.log.Timber
import java.io.File
import java.io.IOException
import java.net.URI

/**
 * Implementation for [BlockDataSource] that provides a way to read requested chunks/blocks from
 * provided file path
 */
internal class FileUriBlockDataSource(
    private val fileUri: URI,
    private val progressObserver: Observer<Int>? = null
) : BlockDataSource {
    override fun getDataSize(offset: Int, size: Int): BlockDataSource.BlockSize {
        if (size < 0) {
            // out of range request
            return BlockDataSource.BlockSize(requestInRange = false)
        }

        var sizeAvailableToRead = 0
        var more = false
        var requestInRange = false

        try {
            File(fileUri).inputStream().use { dataStream ->
                if (dataStream.available() >= offset) {
                    dataStream.skip(offset.toLong())
                    val availableBytes = dataStream.available()
                    sizeAvailableToRead = Math.min(availableBytes, size)
                    more = sizeAvailableToRead < availableBytes
                    requestInRange = true
                }
            }
        } catch (e: IOException) {
            Timber.e(e)
        }

        return BlockDataSource.BlockSize(size = sizeAvailableToRead, more = more, requestInRange = requestInRange)
    }

    override fun getData(offset: Int, size: Int): Data {
        require(offset >= 0) { "offset should be positive" }
        var data = ByteArray(0)

        try {
            File(fileUri).inputStream().use { dataStream ->
                require(size + offset <= dataStream.available()) { "requested size out of range" }
                dataStream.skip(offset.toLong())
                data = ByteArray(size)
                dataStream.read(data)
                progressObserver?.onNext(offset)
            }
        } catch (e: IOException) {
            Timber.e(e)
        }

        return data
    }
}

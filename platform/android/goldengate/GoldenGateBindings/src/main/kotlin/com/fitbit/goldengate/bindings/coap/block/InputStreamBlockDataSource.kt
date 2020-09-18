// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bindings.coap.block

import com.fitbit.goldengate.bindings.coap.data.Data
import io.reactivex.Observer
import timber.log.Timber
import java.io.InputStream

/**
 * Implementation for [BlockDataSource] that provides a way to read requested chunks/blocks from
 * provided data stream
 */
internal class InputStreamBlockDataSource(
    private val dataStream: InputStream,
    private val progressObserver: Observer<Int>? = null
) : BlockDataSource {

    override fun getDataSize(offset: Int, size: Int): BlockDataSource.BlockSize {
        if (size < 0) {
            // out of range request
            return BlockDataSource.BlockSize(requestInRange = false)
        }

        val availableBytes = dataStream.available()
        val sizeAvailableToRead = Math.min(availableBytes, size)
        val more = sizeAvailableToRead < availableBytes

        return BlockDataSource.BlockSize(size = sizeAvailableToRead, more = more)
    }

    override fun getData(offset: Int, size: Int): Data {
        require(offset >= 0) { "offset should be positive" }
        require(size <= dataStream.available()) { "requested size out of range" }
        progressObserver?.onNext(offset)
        val data = ByteArray(size)
        dataStream.read(data)

        return data
    }
}

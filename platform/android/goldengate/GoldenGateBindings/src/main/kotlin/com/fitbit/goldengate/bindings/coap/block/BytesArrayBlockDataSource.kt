// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bindings.coap.block

import com.fitbit.goldengate.bindings.coap.data.Data
import io.reactivex.Observer
import kotlin.math.max
import kotlin.math.min

/**
 * Implementation for [BlockDataSource] that provides a way to read requested chunks/blocks from provided [Data]
 */
internal class BytesArrayBlockDataSource(
    private val data: Data,
    private val progressObserver: Observer<Int>? = null
) : BlockDataSource {

    override fun getDataSize(offset: Int, size: Int): BlockDataSource.BlockSize {
        if (offset < 0 || size < 0 || offset >= data.size) {
            // out of range request
            return BlockDataSource.BlockSize(requestInRange = false)
        }

        val sizeAvailableToRead = max(0, min(size, (data.size - offset)))
        val more = data.size > (offset + sizeAvailableToRead)
        return BlockDataSource.BlockSize(size = sizeAvailableToRead, more = more)
    }

    override fun getData(offset: Int, size: Int): Data {
        require(offset >= 0) { "offset should be positive" }
        require(offset < data.size) { "offset out of range" }
        require((offset + size) <= data.size) { "requested size out of range" }
        progressObserver?.onNext(offset)
        return data.copyOfRange(offset, (offset + size))
    }
}

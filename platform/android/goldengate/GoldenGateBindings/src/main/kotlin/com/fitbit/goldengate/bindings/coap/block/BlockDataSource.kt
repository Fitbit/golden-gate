package com.fitbit.goldengate.bindings.coap.block

import com.fitbit.goldengate.bindings.coap.data.Data

/**
 * Interface implemented by objects that are a source of payload blocks for a CoAP blockwise transfer.
 * This is intended for use from JNI code.
 */
internal interface BlockDataSource {

    /**
     * Block size information returned to native coap layer in response to [BlockDataSource.getDataSize] call
     */
    data class BlockSize(
            /** The max number of bytes that can be read from this block */
            val size: Int = 0,
            /** Whether there's more data to read after this block */
            val more: Boolean = false,
            /** True if the requested block was not outside of the range, FALSE otherwise*/
            val requestInRange: Boolean = true
    )

    /**
     * Get size information for next block that will be read from block source
     *
     * @param offset The offset in source from which to read the data.
     * @param size The max/desired number of bytes to read from that data.
     * @return [BlockSize] size information of block at requested offset
     */
    fun getDataSize(offset: Int, size: Int): BlockSize

    /**
     * Get the data for a given block.
     *
     * @param offset The offset of the requested block in bytes.
     * @param size The size of the requested block in bytes.
     * @return [Data] containing the bytes of the block
     */
    fun getData(offset: Int, size: Int): Data
}

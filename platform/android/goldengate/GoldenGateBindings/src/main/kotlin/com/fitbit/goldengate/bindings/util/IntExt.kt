package com.fitbit.goldengate.bindings.util

import com.fitbit.goldengate.bindings.coap.data.BlockInfo

/**
 *    this extension function helps to decode block option and returns the current block offset
 *    and if it's the last block
 */
fun Int.toBlockInfo(): BlockInfo {
    val num = this ushr 4
    val m = this and 0x8
    val szx = this and 0x7
    val offset = num shl (szx + 4)
    val moreBlock = m != 0

    return BlockInfo(offset, moreBlock)
}

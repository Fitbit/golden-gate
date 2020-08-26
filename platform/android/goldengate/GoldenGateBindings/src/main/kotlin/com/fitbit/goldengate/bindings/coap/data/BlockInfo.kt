package com.fitbit.goldengate.bindings.coap.data

/**
 * the data class to represent the block option values.
 */
data class BlockInfo(val startOffset: Int, val moreBlock: Boolean)

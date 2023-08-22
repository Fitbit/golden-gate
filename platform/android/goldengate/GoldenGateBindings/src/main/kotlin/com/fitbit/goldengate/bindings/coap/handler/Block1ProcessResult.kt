package com.fitbit.goldengate.bindings.coap.handler

/**
 * Enum representing the results of block1 request process
 */
enum class Block1ProcessResult(val value: Int) {
    COMPLETED(0),
    TIMEOUT(1),
    INCORRECT_OFFSET(1)
}

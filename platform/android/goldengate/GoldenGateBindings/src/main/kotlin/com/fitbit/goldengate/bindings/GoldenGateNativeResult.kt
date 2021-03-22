package com.fitbit.goldengate.bindings

private const val GG_ERROR_BASE = -10000

enum class GoldenGateNativeResult(val code: Int, val title: String) {
    GG_SUCCESS                     ((                 0), "the operation or call succeeded"),
    GG_FAILURE                     ((                -1), "an unspecified failure"),
    GG_ERROR_OUT_OF_MEMORY         ((GG_ERROR_BASE -  0), "Out of memory"),
    GG_ERROR_OUT_OF_RESOURCES      ((GG_ERROR_BASE -  1), "Out of resources"),
    GG_ERROR_INTERNAL              ((GG_ERROR_BASE -  2), "Internal error"),
    GG_ERROR_INVALID_PARAMETERS    ((GG_ERROR_BASE -  3), "Invalid parameters"),
    GG_ERROR_INVALID_STATE         ((GG_ERROR_BASE -  4), "Invalid state"),
    GG_ERROR_NOT_IMPLEMENTED       ((GG_ERROR_BASE -  5), "Not implemented"),
    GG_ERROR_OUT_OF_RANGE          ((GG_ERROR_BASE -  6), "Out of range"),
    GG_ERROR_ACCESS_DENIED         ((GG_ERROR_BASE -  7), "Access denied"),
    GG_ERROR_INVALID_SYNTAX        ((GG_ERROR_BASE -  8), "Invalid syntax"),
    GG_ERROR_NOT_SUPPORTED         ((GG_ERROR_BASE -  9), "Not supported"),
    GG_ERROR_INVALID_FORMAT        ((GG_ERROR_BASE - 10), "Invalid format"),
    GG_ERROR_NOT_ENOUGH_SPACE      ((GG_ERROR_BASE - 11), "Not enough space"),
    GG_ERROR_NO_SUCH_ITEM          ((GG_ERROR_BASE - 12), "No such item (item not found)"),
    GG_ERROR_OVERFLOW              ((GG_ERROR_BASE - 13), "Overflow"),
    GG_ERROR_TIMEOUT               ((GG_ERROR_BASE - 14), "A timeout occurred"),
    GG_ERROR_WOULD_BLOCK           ((GG_ERROR_BASE - 15), "The operation would block / try again later"),
    GG_ERROR_PERMISSION_DENIED     ((GG_ERROR_BASE - 16), "Permission denied"),
    GG_ERROR_INTERRUPTED           ((GG_ERROR_BASE - 17), "Operation interrupted"),
    GG_ERROR_IN_USE                ((GG_ERROR_BASE - 18), "Resource already in use"),
    GG_ERROR_BASE_IO               ((GG_ERROR_BASE - 100), "I/O related error"),
    GG_ERROR_BASE_SOCKET           ((GG_ERROR_BASE - 200), "Socket related error"),
    GG_ERROR_BASE_COAP             ((GG_ERROR_BASE - 300), "CoAP related error"),
    GG_ERROR_BASE_REMOTE           ((GG_ERROR_BASE - 400), "Remote API related error"),
    GG_ERROR_BASE_GATTLINK         ((GG_ERROR_BASE - 500), "Gattlink related error"),
    GG_ERROR_BASE_TLS              ((GG_ERROR_BASE - 600), "TLS related error");

    val nameSpace = "gg.result"

    companion object {
        fun getNativeResultFrom(code: Int): GoldenGateNativeResult{
            return when(code) {
                in 0 .. Int.MAX_VALUE -> return GG_SUCCESS
                in -10099 .. -1 -> {
                    for (value in values()) {
                        if (value.code == code) return value
                    }
                    GG_FAILURE
                }
                in -10199 .. -10100 -> GG_ERROR_BASE_IO
                in -10299 .. -10200 -> GG_ERROR_BASE_SOCKET
                in -10399 .. -10300 -> GG_ERROR_BASE_COAP
                in -10499 .. -10400 -> GG_ERROR_BASE_REMOTE
                in -10599 .. -10500 -> GG_ERROR_BASE_GATTLINK
                in -10699 .. -10600 -> GG_ERROR_BASE_TLS
                else -> GG_FAILURE
            }
        }
    }
}

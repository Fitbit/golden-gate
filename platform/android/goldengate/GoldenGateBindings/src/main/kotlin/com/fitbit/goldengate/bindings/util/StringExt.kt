package com.fitbit.goldengate.bindings.util

private const val HEX_CHARS = "0123456789ABCDEF"

/**
 * Extension function to convert hex string to byte array
 */
fun String.hexStringToByteArray(): ByteArray {
    val dataString = this.toUpperCase()
    val result = ByteArray(this.length / 2)

    for (i in 0 until this.length step 2) {
        val octet = ((HEX_CHARS.indexOf(dataString[i]) shl 4)
                + HEX_CHARS.indexOf(dataString[i + 1]))
                .toByte()
        result[i.shr(1)] = octet
    }
    return result
}

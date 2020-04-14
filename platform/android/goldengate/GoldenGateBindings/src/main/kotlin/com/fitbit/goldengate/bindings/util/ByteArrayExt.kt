package com.fitbit.goldengate.bindings.util

import com.fitbit.goldengate.bindings.BuildConfig

/**
 * NOTE: returns "" (i.e. the empty string) in release versions of the library
 */
fun ByteArray.hexString(): String {
    return if (BuildConfig.DEBUG) {
        val builder = StringBuilder()

        for (b in this) {
            val st = String.format("%02X ", b)
            builder.append(st)
        }

        builder.toString()
    } else {
        ""
    }
}

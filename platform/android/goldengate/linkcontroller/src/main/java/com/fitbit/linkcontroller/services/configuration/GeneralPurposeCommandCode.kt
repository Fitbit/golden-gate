package com.fitbit.linkcontroller.services.configuration

enum class GeneralPurposeCommandCode(private val byte: Byte) {
    RESERVED(0),
    DISCONNECT(1);

    fun toByteArray(): ByteArray {
        val byteArray = ByteArray(1)
        byteArray[0] = byte
        return byteArray
    }
}

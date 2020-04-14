package com.fitbit.goldengate.bindings.remote

interface CborHandler {

    val name: String

    /**
     * @return a ByteArray containing the result, or null if there was an error.
     * @throws IllegalArgumentException if the requestParams do not conform to expectations.
     */
    fun handle(requestParamsCbor: ByteArray) : ByteArray
}

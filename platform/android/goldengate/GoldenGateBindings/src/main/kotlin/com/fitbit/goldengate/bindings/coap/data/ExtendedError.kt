package com.fitbit.goldengate.bindings.coap.data

data class ExtendedError (
    /**
     * Namespace for the error code (ex: "org.example.foo")
     */
    val namespace: String? = null,
    /**
     * Error code
     */
    val code: Int,
    /**
     * Error message
     */
    val message: String? = null
)

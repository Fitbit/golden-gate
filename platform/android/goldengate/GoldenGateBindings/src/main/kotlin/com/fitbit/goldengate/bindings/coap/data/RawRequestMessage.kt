package com.fitbit.goldengate.bindings.coap.data

/**
 * Data containing message for a block or single coap response.
 *
 * This class is used to transfer coap message from jni
 */
internal class RawRequestMessage(
        val method: Method,
        val options: Options,
        val data: Data
)

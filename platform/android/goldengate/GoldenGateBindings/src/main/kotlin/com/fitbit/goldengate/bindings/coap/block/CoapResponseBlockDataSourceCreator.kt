package com.fitbit.goldengate.bindings.coap.block

import com.fitbit.goldengate.bindings.coap.data.BytesArrayOutgoingBody
import com.fitbit.goldengate.bindings.coap.data.EmptyOutgoingBody
import com.fitbit.goldengate.bindings.coap.data.InputStreamOutgoingBody
import com.fitbit.goldengate.bindings.coap.data.OutgoingResponse

/**
 * Helper called from JNI to provide a convenient way of creating [BlockDataSource] for CoAP response
 */
internal class CoapResponseBlockDataSourceCreator {
    /**
     * Create a new instance of [BlockDataSource] from given message
     *
     * @param response outgoing coap response message
     * @return new instance of [BlockDataSource]
     */
    fun create(response: OutgoingResponse): BlockDataSource? {
        val body = response.body
        return when (body) {
            is InputStreamOutgoingBody -> InputStreamBlockDataSource(body.data)
            is BytesArrayOutgoingBody -> BytesArrayBlockDataSource(body.data)
            is EmptyOutgoingBody -> BytesArrayBlockDataSource(body.data)
        }
    }
}

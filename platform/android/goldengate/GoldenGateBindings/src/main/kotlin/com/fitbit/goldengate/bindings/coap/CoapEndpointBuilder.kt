package com.fitbit.goldengate.bindings.coap

/**
 * Utility object for building a [CoapEndpoint]
 */
object CoapEndpointBuilder: () -> CoapEndpoint {

    /**
     * Get a new instance of a [CoapEndpoint]
     */
    override fun invoke(): CoapEndpoint = CoapEndpoint()
}

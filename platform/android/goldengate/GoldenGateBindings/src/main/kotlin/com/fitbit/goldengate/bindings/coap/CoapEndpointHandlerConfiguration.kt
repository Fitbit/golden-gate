package com.fitbit.goldengate.bindings.coap

/**
 * Contains all the configuration/flags used by CoAP endpoint handler registration
 */
data class CoapEndpointHandlerConfiguration(val filterGroup: CoapGroupRequestFilterMode = CoapGroupRequestFilterMode.GROUP_0)
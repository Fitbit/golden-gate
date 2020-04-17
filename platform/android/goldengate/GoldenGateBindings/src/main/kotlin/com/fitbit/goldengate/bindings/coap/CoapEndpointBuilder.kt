// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

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

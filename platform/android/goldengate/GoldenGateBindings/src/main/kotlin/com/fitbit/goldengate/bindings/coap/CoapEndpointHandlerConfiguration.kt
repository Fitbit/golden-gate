// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bindings.coap

/**
 * Contains all the configuration/flags used by CoAP endpoint handler registration
 */
data class CoapEndpointHandlerConfiguration(val filterGroup: CoapGroupRequestFilterMode = CoapGroupRequestFilterMode.GROUP_0)

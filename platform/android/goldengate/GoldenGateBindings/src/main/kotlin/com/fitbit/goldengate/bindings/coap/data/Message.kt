// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bindings.coap.data

/**
 * This interface represents CoAP message objects.
 */
interface Message {
    /**
     * Coap message options
     */
    val options: Options
}

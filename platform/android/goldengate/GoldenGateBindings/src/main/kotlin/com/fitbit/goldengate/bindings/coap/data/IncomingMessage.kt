// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bindings.coap.data

/**
 * This interface represents incoming CoAP [Message] objects.
 */
interface IncomingMessage: Message {
    /**
     * Coap message body
     */
    val body: IncomingBody
}

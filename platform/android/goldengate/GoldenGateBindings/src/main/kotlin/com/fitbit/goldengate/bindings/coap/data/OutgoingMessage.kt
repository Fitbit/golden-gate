// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bindings.coap.data

/**
 * This interface represents outgoing CoAP [Message] objects.
 */
interface OutgoingMessage: Message {
    /**
     * Coap message body
     */
    val body: OutgoingBody
}

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

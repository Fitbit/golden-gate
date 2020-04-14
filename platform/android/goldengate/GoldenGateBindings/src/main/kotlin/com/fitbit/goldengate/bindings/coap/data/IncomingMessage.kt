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

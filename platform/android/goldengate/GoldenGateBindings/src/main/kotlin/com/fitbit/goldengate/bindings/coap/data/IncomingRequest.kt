package com.fitbit.goldengate.bindings.coap.data

/**
 * Incoming coap request message.
 *
 * Coap server will receive this request object for any client request.
 */
interface IncomingRequest : IncomingMessage, BaseRequest

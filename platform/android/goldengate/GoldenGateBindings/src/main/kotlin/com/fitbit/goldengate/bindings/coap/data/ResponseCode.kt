// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bindings.coap.data

/**
 * This represents a CoAP response code (analogous to an HTTP response code (e.g. 201, 404, 500)
 *
 * @property responseClass Response [Class] is the major class of the code. 2 = Success, 3 = Redirection, 4 = Client Error, 5 = Server Error
 * @property detail is the class-specific detail code. (e.g. ResponseCode(2, 5) represents CONTENT and ResponseCode (4, 4) represents NOT_FOUND
 *
 * @see <a href="https://tools.ietf.org/html/rfc7252#section-5.9">CoAP Response Code Definitions</a>
 */
data class ResponseCode constructor(val responseClass: Byte, val detail: Byte) {

    object Class {
        const val ok = 2.toByte()
        const val clientError = 4.toByte()
        const val serverError = 5.toByte()
    }

    companion object {
        /** CoAP Response Code 201 */ val created                   = ResponseCode(Class.ok, 1)
        /** CoAP Response Code 202 */ val deleted                   = ResponseCode(Class.ok, 2)
        /** CoAP Response Code 203 */ val valid                     = ResponseCode(Class.ok, 3)
        /** CoAP Response Code 204 */ val changed                   = ResponseCode(Class.ok, 4)
        /** CoAP Response Code 205 */ val content                   = ResponseCode(Class.ok, 5)
        /** CoAP Response Code 231 */ val `continue`                = ResponseCode(Class.ok, 31)

        /** CoAP Response Code 400 */ val badRequest                = ResponseCode(Class.clientError, 0)
        /** CoAP Response Code 401 */ val unauthorized              = ResponseCode(Class.clientError, 1)
        /** CoAP Response Code 402 */ val badOption                 = ResponseCode(Class.clientError, 2)
        /** CoAP Response Code 403 */ val forbidden                 = ResponseCode(Class.clientError, 3)
        /** CoAP Response Code 404 */ val notFound                  = ResponseCode(Class.clientError, 4)
        /** CoAP Response Code 405 */ val methodNotAllowed          = ResponseCode(Class.clientError, 5)
        /** CoAP Response Code 406 */ val notAcceptable             = ResponseCode(Class.clientError, 6)
        /** CoAP Response Code 408 */ val requestEntityIncomplete   = ResponseCode(Class.clientError, 8)
        /** CoAP Response Code 412 */ val preconditionFailed        = ResponseCode(Class.clientError, 12)
        /** CoAP Response Code 413 */ val requestEntityTooLarge     = ResponseCode(Class.clientError, 13)
        /** CoAP Response Code 415 */ val unsupportedContentFormat  = ResponseCode(Class.clientError, 15)

        /** CoAP Response Code 500 */ val internalServerError       = ResponseCode(Class.serverError, 0)
        /** CoAP Response Code 501 */ val notImplemented            = ResponseCode(Class.serverError, 1)
        /** CoAP Response Code 502 */ val badGateway                = ResponseCode(Class.serverError, 2)
        /** CoAP Response Code 503 */ val serviceUnavailable        = ResponseCode(Class.serverError, 3)
        /** CoAP Response Code 504 */ val gatewayTimeout            = ResponseCode(Class.serverError, 4)
        /** CoAP Response Code 505 */ val proxyingNotSupported      = ResponseCode(Class.serverError, 5)
    }
}

/**
 * Extension function that return true if response is ok, false otherwise
 */
fun ResponseCode.ok(): Boolean = responseClass == ResponseCode.Class.ok

/**
 * Extension function that return true if response is client error, false otherwise
 */
fun ResponseCode.clientError(): Boolean = responseClass == ResponseCode.Class.clientError

/**
 * Extension function that return true if response is server error, false otherwise
 */
fun ResponseCode.serverError(): Boolean = responseClass == ResponseCode.Class.serverError

/**
 * Extension function that return true if response is client or server error, false otherwise
 */
fun ResponseCode.error(): Boolean = clientError() || serverError()

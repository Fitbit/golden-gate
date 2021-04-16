package com.fitbit.goldengate.bindings

private const val GG_ERROR_BASE = -10000

enum class GoldenGateNativeResult(val code: Int, val title: String) {
    GG_SUCCESS                        ((                 0), "the operation or call succeeded"),
    GG_FAILURE                        ((                -1), "an unspecified failure"),
    GG_ERROR_OUT_OF_MEMORY            ((GG_ERROR_BASE -  0), "Out of memory"),
    GG_ERROR_OUT_OF_RESOURCES         ((GG_ERROR_BASE -  1), "Out of resources"),
    GG_ERROR_INTERNAL                 ((GG_ERROR_BASE -  2), "Internal error"),
    GG_ERROR_INVALID_PARAMETERS       ((GG_ERROR_BASE -  3), "Invalid parameters"),
    GG_ERROR_INVALID_STATE            ((GG_ERROR_BASE -  4), "Invalid state"),
    GG_ERROR_NOT_IMPLEMENTED          ((GG_ERROR_BASE -  5), "Not implemented"),
    GG_ERROR_OUT_OF_RANGE             ((GG_ERROR_BASE -  6), "Out of range"),
    GG_ERROR_ACCESS_DENIED            ((GG_ERROR_BASE -  7), "Access denied"),
    GG_ERROR_INVALID_SYNTAX           ((GG_ERROR_BASE -  8), "Invalid syntax"),
    GG_ERROR_NOT_SUPPORTED            ((GG_ERROR_BASE -  9), "Not supported"),
    GG_ERROR_INVALID_FORMAT           ((GG_ERROR_BASE - 10), "Invalid format"),
    GG_ERROR_NOT_ENOUGH_SPACE         ((GG_ERROR_BASE - 11), "Not enough space"),
    GG_ERROR_NO_SUCH_ITEM             ((GG_ERROR_BASE - 12), "No such item (item not found)"),
    GG_ERROR_OVERFLOW                 ((GG_ERROR_BASE - 13), "Overflow"),
    GG_ERROR_TIMEOUT                  ((GG_ERROR_BASE - 14), "A timeout occurred"),
    GG_ERROR_WOULD_BLOCK              ((GG_ERROR_BASE - 15), "The operation would block / try again later"),
    GG_ERROR_PERMISSION_DENIED        ((GG_ERROR_BASE - 16), "Permission denied"),
    GG_ERROR_INTERRUPTED              ((GG_ERROR_BASE - 17), "Operation interrupted"),
    GG_ERROR_IN_USE                   ((GG_ERROR_BASE - 18), "Resource already in use"),

    GG_ERROR_EOS                      ((GG_ERROR_BASE - 100), "End Of Stream"),

    GG_ERROR_CONNECTION_RESET         ((GG_ERROR_BASE - 200), "Connection reset"),
    GG_ERROR_CONNECTION_ABORTED       ((GG_ERROR_BASE - 201), "Connection aborted"),
    GG_ERROR_CONNECTION_REFUSED       ((GG_ERROR_BASE - 202), "Connection refused"),
    GG_ERROR_CONNECTION_FAILED        ((GG_ERROR_BASE - 203), "Connection failed"),
    GG_ERROR_HOST_UNKNOWN             ((GG_ERROR_BASE - 204), "Host unknown"),
    GG_ERROR_SOCKET_FAILED            ((GG_ERROR_BASE - 205), "Socket failed"),
    GG_ERROR_GETSOCKOPT_FAILED        ((GG_ERROR_BASE - 206), "Getsockopt() failed"),
    GG_ERROR_SETSOCKOPT_FAILED        ((GG_ERROR_BASE - 207), "Setsockopt() failed"),
    GG_ERROR_SOCKET_CONTROL_FAILED    ((GG_ERROR_BASE - 208), "Control failed"),
    GG_ERROR_BIND_FAILED              ((GG_ERROR_BASE - 209), "Bind failed"),
    GG_ERROR_LISTEN_FAILED            ((GG_ERROR_BASE - 210), "Listen failed"),
    GG_ERROR_ACCEPT_FAILED            ((GG_ERROR_BASE - 211), "Accept failed"),
    GG_ERROR_ADDRESS_IN_USE           ((GG_ERROR_BASE - 212), "Address in use"),
    GG_ERROR_NETWORK_DOWN             ((GG_ERROR_BASE - 213), "Network down"),
    GG_ERROR_NETWORK_UNREACHABLE      ((GG_ERROR_BASE - 214), "Network unreachable"),
    GG_ERROR_HOST_UNREACHABLE         ((GG_ERROR_BASE - 215), "Host unreachable"),
    GG_ERROR_NOT_CONNECTED            ((GG_ERROR_BASE - 216), "Not connected"),
    GG_ERROR_UNMAPPED_STACK_ERROR     ((GG_ERROR_BASE - 217), "Unmapped stack error"),

    GG_ERROR_COAP_UNSUPPORTED_VERSION ((GG_ERROR_BASE - 300), "CoAP unsupported version"),
    GG_ERROR_COAP_RESET               ((GG_ERROR_BASE - 301), "CoAP reset"),
    GG_ERROR_COAP_UNEXPECTED_MESSAGE  ((GG_ERROR_BASE - 302), "CoAP unexpected message"),
    GG_ERROR_COAP_SEND_FAILURE        ((GG_ERROR_BASE - 303), "CoAP send failure"),
    GG_ERROR_COAP_UNEXPECTED_BLOCK    ((GG_ERROR_BASE - 304), "CoAP unexpected block"),
    GG_ERROR_COAP_INVALID_RESPONSE    ((GG_ERROR_BASE - 305), "CoAP invalid response"),
    GG_ERROR_COAP_ETAG_MISMATCH       ((GG_ERROR_BASE - 306), "CoAP E-Tag mismatch"),

    GG_ERROR_REMOTE_EXIT              ((GG_ERROR_BASE - 400), "Remote API shell exited"),

    GG_ERROR_GATTLINK_UNEXPECTED_PSN  ((GG_ERROR_BASE - 500), "Gattlink unexpected PSN"),

    GG_ERROR_TLS_FATAL_ALERT_MESSAGE  ((GG_ERROR_BASE - 600), "TLS FATAL alert message"),
    GG_ERROR_TLS_UNKNOWN_IDENTITY     ((GG_ERROR_BASE - 601), "TLS unknown identity"),
    GG_ERROR_TLS_BAD_CLIENT_HELLO     ((GG_ERROR_BASE - 602), "TLS bad client hello"),
    GG_ERROR_TLS_BAD_SERVER_HELLO     ((GG_ERROR_BASE - 603), "TLS bad server hello"),
    GG_ERROR_TLS_UNMAPPED_LIB_ERROR   ((GG_ERROR_BASE - 604), "TLS unmapped lib error"),

    GG_ERROR_ERRNO                    ((GG_ERROR_BASE - 2000), "a wrapped errno");

    companion object {
        fun getNativeResultFrom(code: Int): GoldenGateNativeResult{
            return when(code) {
                in 0 .. Int.MAX_VALUE -> return GG_SUCCESS
                in -10699 .. -1 -> {
                    for (value in values()) {
                        if (value.code == code) return value
                    }
                    GG_FAILURE
                }
                in -12999 .. -12000 -> GG_ERROR_ERRNO
                else -> GG_FAILURE
            }
        }
    }
}

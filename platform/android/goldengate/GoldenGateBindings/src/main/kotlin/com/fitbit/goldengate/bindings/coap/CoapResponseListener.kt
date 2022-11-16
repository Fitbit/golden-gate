// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bindings.coap

import com.fitbit.goldengate.bindings.coap.data.RawResponseMessage

/**
 * Listener called from native layer for to deliver response to caller/client for a coap request.
 *
 * Methods in this interface are called from C via reflection.
 * Do not change any names or signatures without updating jni_gg_coap_client_common.h
 */
internal interface CoapResponseListener {

    /**
     * Method called when an ACK is received
     */
    fun onAck()

    /**
     * Method called when an error has occurred.
     *
     * @param error Error code
     * @param message Error message text
     */
    fun onError(error: Int, message: String)

    /**
     * Method called when a block or single response is received.
     *
     * @param message coap response message
     */
    fun onNext(message: RawResponseMessage)

    /**
     * Method called response from server is successfully completed
     */
    fun onComplete()

    /**
     * Returns True if complete response if already received (either successfully or with error), False otherwise
     */
    fun isComplete(): Boolean

    /**
     * Method called to set the native reference of the jni coap listener object
     */
    fun setNativeListenerReference(nativeReference: Long)

    /**
     * Method called to clean up the native reference of the jni coap listener object and cancel the request if necessary
     */
    fun cleanupNativeListener()
}

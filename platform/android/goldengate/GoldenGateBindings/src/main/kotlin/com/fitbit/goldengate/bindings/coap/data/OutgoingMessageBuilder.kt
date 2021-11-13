// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bindings.coap.data

import io.reactivex.Observer
import java.io.InputStream
import java.net.URI

/**
 * A MessageBuilder is used for constructing Message objects. This interface contains the shared components of
 * [OutgoingRequestBuilder] and [OutgoingResponseBuilder]
 */
interface OutgoingMessageBuilder<T : Message> {

    /**
     * Add an option to coap message
     *
     * @param option one of the supported [Option]
     */
    fun option(option: Option): OutgoingMessageBuilder<T>

    /**
     * Body of the coap message
     *
     * @param data ByteArray containing coap message body
     */
    fun body(data: Data): OutgoingMessageBuilder<T>

    /**
     * Body of the coap message
     *
     * @param stream stream containing coap message body
     */
    fun body(stream: InputStream): OutgoingMessageBuilder<T>

    /**
     * Body of the coap message
     *
     * @param fileUri file containing coap message body
     */
    fun body(fileUri: URI): OutgoingMessageBuilder<T>

    /**
     * Progress Observer of the coap message
     *
     * @param progressObserver an observer for bytes sent, this will be triggered by BlockDataSource when getData is called
     * This observer will complete when the request is finished.
     */
    fun progressObserver(progressObserver: Observer<Int>): OutgoingMessageBuilder<T>

    /**
     * Build Coap [Message]
     */
    fun build(): T
}

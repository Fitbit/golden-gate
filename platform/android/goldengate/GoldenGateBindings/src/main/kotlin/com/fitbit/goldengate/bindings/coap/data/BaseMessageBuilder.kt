// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bindings.coap.data

import io.reactivex.Observer
import io.reactivex.internal.util.EmptyComponent
import java.io.InputStream
import java.net.URI

/**
 * Base class for [MessageBuilder] providing common implementation
 */
abstract class BaseOutgoingMessageBuilder<T : Message> : OutgoingMessageBuilder<T> {

    internal val options = Options()
    internal var body: OutgoingBody = EmptyOutgoingBody()
    internal var progressObserver: Observer<Int> = EmptyComponent.asObserver<Int>()

    override fun option(option: Option): OutgoingMessageBuilder<T> {
        options.add(option)
        return this
    }

    override fun body(data: Data): OutgoingMessageBuilder<T> {
        body = BytesArrayOutgoingBody(data)
        return this
    }

    override fun body(stream: InputStream): OutgoingMessageBuilder<T> {
        body = InputStreamOutgoingBody(stream)
        return this
    }

    override fun body(fileUri: URI): OutgoingMessageBuilder<T> {
        body = FileUriOutgoingBody(fileUri)
        return this
    }

    override fun progressObserver(progressObserver: Observer<Int>): OutgoingMessageBuilder<T> {
        this.progressObserver = progressObserver
        return this
    }

    internal fun addAll(options: Options): OutgoingMessageBuilder<T> {
        this.options.addAll(options)
        return this
    }

}

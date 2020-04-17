// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bindings.coap.data

import java.io.InputStream

/**
 * Represents body of outgoing coap message.
 *
 * In server mode its the response and in client mode its the request
 */
sealed class OutgoingBody

/**
 * Empty [OutgoingBody]
 */
class EmptyOutgoingBody private constructor(val data: Data) : OutgoingBody() {
    constructor() : this(Data(0))
}

/**
 * [OutgoingBody] with body source from a [ByteArray]
 *
 * @property data the data to send with the request that has this body
 */
class BytesArrayOutgoingBody internal constructor(val data: Data) : OutgoingBody()

/**
 * [OutgoingBody] with body source from a [InputStream]
 *
 * @property data an [InputStream] that will provide the data to send with the request that has this body
 */
class InputStreamOutgoingBody internal constructor(val data: InputStream) : OutgoingBody()

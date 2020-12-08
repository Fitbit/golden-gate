// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bindings.services

import com.fitbit.goldengate.bindings.NativeReference
import com.fitbit.goldengate.bindings.coap.CoapEndpoint
import com.fitbit.goldengate.bindings.coap.CoapEndpointBuilder
import com.fitbit.goldengate.bindings.remote.RemoteShellThread
import java.io.Closeable

/**
 * Service that sends CoAP requests using a CoAP endpoint
 */
class CoapGeneratorService private constructor(
    private val remoteShellThread: RemoteShellThread,
    private val endPoint: CoapEndpoint
) : NativeReference, Closeable {

    override val thisPointer: Long

    init {
        thisPointer = create()
        // need to register this service since we will communicate via the remote shell
        register()
    }

    class Provider(private val remoteShellThread: RemoteShellThread) {
        fun get(endpoint: CoapEndpoint = CoapEndpointBuilder()) =
            CoapGeneratorService(remoteShellThread, endpoint)
    }

    override fun close() {
        destroy()
    }

    private external fun create(endpointPtrWrapper: Long = endPoint.thisPointerWrapper): Long

    private external fun register(selfPtr: Long = thisPointer, shellPtr: Long = remoteShellThread.thisPointer)

    private external fun destroy(selfPtr: Long = thisPointer)
}

// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bindings.services

import com.fitbit.goldengate.bindings.NativeReference
import com.fitbit.goldengate.bindings.coap.CoapEndpoint
import com.fitbit.goldengate.bindings.coap.CoapEndpointBuilder
import com.fitbit.goldengate.bindings.remote.RemoteShellThread
import java.io.Closeable

/**
 * Reusable cross-platform CoAP server that can be used for automated functionality and performance testing
 */
class CoapTestService(private val remoteShellThread: RemoteShellThread,
                      private val endPoint: CoapEndpoint
): NativeReference, Closeable {

    override val thisPointer: Long

    init {
        thisPointer = create()
        register()
    }

    override fun close() {
        destroy()
    }

    class Provider(private val remoteShellThread: RemoteShellThread) {
        fun get(endPoint: CoapEndpoint = CoapEndpointBuilder()) =
            CoapTestService(remoteShellThread, endPoint)
    }

    private external fun create(endPointPtr:Long = endPoint.thisPointer):Long

    private external fun register(selfPtr:Long = thisPointer, remoteShellPtr:Long = remoteShellThread.thisPointer)

    private external fun destroy(selfPtr:Long = thisPointer):Long
}

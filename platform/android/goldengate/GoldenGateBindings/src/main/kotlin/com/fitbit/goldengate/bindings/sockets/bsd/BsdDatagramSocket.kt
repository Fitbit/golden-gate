// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bindings.sockets.bsd

import com.fitbit.goldengate.bindings.DataSinkDataSource
import com.fitbit.goldengate.bindings.NativeReference
import java.io.Closeable
import java.net.Inet4Address
import java.net.InetAddress
import java.net.InetSocketAddress

/**
 * Creates a new BSD socket with given configuration data
 *
 * @param address address of a BSD socket
 */
class BsdDatagramSocket(
        address: BsdDatagramSocketAddress
) : NativeReference, DataSinkDataSource, Closeable {

    override val thisPointer: Long

    init {
        verifyRemoteAddress(address.remoteAddress)

        thisPointer = create(
                localPort = address.localPort,
                remoteIpAddress = address.remoteAddress?.address,
                remotePort = address.remoteAddress?.port ?: 0
        )
    }

    override fun getAsDataSinkPointer() = asDataSink()

    override fun getAsDataSourcePointer() = asDataSource()

    override fun close() {
        destroy()
    }

    private fun verifyRemoteAddress(remoteAddress: InetSocketAddress?) {
        remoteAddress?.let {
            require(it.address is Inet4Address) {
                "Only IPv4 address is supported"
            }
        }
    }

    private external fun create(
            localPort: Int,
            remoteIpAddress: InetAddress?,
            /**
             * remotePort is intentionally non-null otherwise, making it nullable would require
             * jni side to be jobject instead of jint(as nullable Int is not correctly casted
             * to jint)
             */
            remotePort: Int
    ): Long

    private external fun destroy(selfPtr: Long = thisPointer)

    private external fun asDataSource(selfPtr: Long = thisPointer): Long

    private external fun asDataSink(selfPtr: Long = thisPointer): Long

}

// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bindings.sockets.bsd

import java.net.InetSocketAddress

/**
 * Data representing address for both end of a BSD socket
 *
 * @param localPort     The local port this socket should be bound to
 * @param remoteAddress The remote address this socket should be bound to. When remote address is
 *                      specified all request will be sent to that address. If NULL this socket
 *                      can only receive request.
 */
data class BsdDatagramSocketAddress(
        val localPort: Int,
        val remoteAddress: InetSocketAddress? = null
)

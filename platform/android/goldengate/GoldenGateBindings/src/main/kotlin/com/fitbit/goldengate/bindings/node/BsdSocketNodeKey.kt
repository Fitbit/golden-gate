// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bindings.node

import com.fitbit.goldengate.bindings.sockets.bsd.BsdDatagramSocketAddress

/**
 * Uniquely identifies a Node that is connected over BsdSocket
 */
data class BsdSocketNodeKey(
        override val value: BsdDatagramSocketAddress
) : NodeKey<BsdDatagramSocketAddress>

package com.fitbit.goldengate.bindings.node

import com.fitbit.goldengate.bindings.sockets.bsd.BsdDatagramSocketAddress

/**
 * Uniquely identifies a Node that is connected over BsdSocket
 */
data class BsdSocketNodeKey(
        override val value: BsdDatagramSocketAddress
) : NodeKey<BsdDatagramSocketAddress>

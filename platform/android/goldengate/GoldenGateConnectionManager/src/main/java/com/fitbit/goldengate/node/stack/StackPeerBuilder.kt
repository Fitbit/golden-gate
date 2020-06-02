package com.fitbit.goldengate.node.stack

import com.fitbit.goldengate.bindings.node.BluetoothAddressNodeKey
import com.fitbit.goldengate.bindings.stack.Stack
import com.fitbit.goldengate.bindings.stack.StackConfig
import com.fitbit.goldengate.bindings.stack.StackService
import com.fitbit.goldengate.bt.PeerRole
import com.fitbit.goldengate.node.Peer
import com.fitbit.goldengate.node.PeerBuilder
import io.reactivex.Single

/**
 * Builds a [StackPeer] with a [String] key
 *
 * @param clazz The [Class] of the [StackService] to use with this [StackPeer]
 * @param stackConfig the [StackConfig] of the [StackPeer]'s [Stack]
 * @param buildStackPeer function for building a [StackPeer]
 */
class StackPeerBuilder<T: StackService>(
    private val clazz: Class<in T>,
    private val peerRole: PeerRole,
    private val stackConfig: StackConfig,
    private val buildStackPeer: (BluetoothAddressNodeKey) -> StackPeer<T>
): PeerBuilder<T, BluetoothAddressNodeKey> {

    /**
     * Utility constructor providing a default function to build a stack
     */
    internal constructor(
        clazz: Class<in T>,
        peerRole: PeerRole,
        stackServiceProvider: () -> T,
        stackConfig: StackConfig
    ): this(clazz, peerRole, stackConfig, { nodeKey ->
        StackPeer(nodeKey, peerRole, stackConfig, stackServiceProvider())
    })

    /**
     * Build the [StackPeer], connect it and emit it via a [Single]
     */
    override fun build(key: BluetoothAddressNodeKey) : StackPeer<T> = buildStackPeer(key)

    override fun doesBuild(peer: Peer<*>): Boolean =
        peer is StackPeer
            && peer.stackConfig == stackConfig
            && peer.stackService::class.java == clazz
            && peer.peerRole == peerRole
}

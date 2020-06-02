package com.fitbit.goldengate.node.stack

import com.fitbit.goldengate.bindings.stack.StackConfig
import com.fitbit.goldengate.bindings.stack.StackService
import com.fitbit.goldengate.bt.PeerRole

/**
 * Used to expose building a stack for use with the Remote API
 */
object RemoteApiStackPeerBuilder {
    operator fun <T: StackService> invoke(
        clazz: Class<T>,
        peerRole: PeerRole,
        stackConfig: StackConfig,
        stackServiceProvider: () -> T
    ): StackPeerBuilder<T> = StackPeerBuilder(clazz, peerRole, stackConfig) {
        StackPeer(it, peerRole, stackConfig, stackServiceProvider())
    }
}

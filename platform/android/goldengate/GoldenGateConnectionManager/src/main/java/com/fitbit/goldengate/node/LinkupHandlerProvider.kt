package com.fitbit.goldengate.node

import com.fitbit.goldengate.bt.PeerRole
import com.fitbit.goldengate.hub.LinkupWithPeerHubHandler

/**
 * Helper object to return the linkup handler based upon connection role (node or hub)
 */
object LinkupHandlerProvider {

    fun getHandler(peerRole: PeerRole): Linkup {
        return when(peerRole) {
            is PeerRole.Peripheral -> LinkupWithPeerNodeHandler()
            is PeerRole.Central -> LinkupWithPeerHubHandler()
        }
    }
}

package com.fitbit.goldengate.node

import com.fitbit.goldengate.bt.PeerRole
import com.fitbit.goldengate.hub.LinkupWithPeerHubHandler
import org.junit.Test

class LinkupHandlerProviderTest {
    @Test
    fun `get the corresponding handler`() {
        val handler1 = LinkupHandlerProvider.getHandler(PeerRole.Peripheral)
        assert(handler1 is LinkupWithPeerNodeHandler)

        val handler2 = LinkupHandlerProvider.getHandler(PeerRole.Central)
        assert(handler2 is LinkupWithPeerHubHandler)
    }
}

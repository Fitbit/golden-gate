package com.fitbit.linkcontroller.services.configuration

/**
 * Listener that is invoked when a Link Configuration Service receives a GATT request from Peer
 */
interface LinkConfigurationPeerRequestListener {

    /**
     * will be called if a GATT read request from Peer is received
     */
    fun onPeerReadRequest()
}

package com.fitbit.remoteapi

import co.nstant.`in`.cbor.CborBuilder
import co.nstant.`in`.cbor.model.Map
import co.nstant.`in`.cbor.model.SimpleValue
import co.nstant.`in`.cbor.model.UnicodeString
import co.nstant.`in`.cbor.model.UnsignedInteger
import com.fitbit.remoteapi.handlers.StartPairingRpc
import org.junit.Test
import kotlin.test.assertEquals

const val MAC_ADDRESS = "AA:BB:CC:DD:EE:FF"

class StartPairingRpcHandlerTest {

    private val peerIdParam = "peer"
    private val idParam = "id"
    private val typeParam = "type"

    @Test
    fun testCommandParsing() {
        // { "peer_id" : {id: "AA:BB:CC:DD:EE:FF", type: "macAddress"} }
        val dataItems = CborBuilder().addMap().putMap("peer").put(idParam, MAC_ADDRESS)
            .put(typeParam, "macAddress").end().end().build()
        val expectedPeerId = PeerId(MAC_ADDRESS, PeerIdType.MAC_ADDRESS)
        assertEquals(expectedPeerId, StartPairingRpc.Parser().parse(dataItems))
    }

    @Test(expected = IllegalArgumentException::class)
    fun testThrowsOnInvalidParamKeyAndValueType() {
        // { "invalid" : 2 }
        val dataItems = CborBuilder().addMap().put("invalid", 2).end().build()
        StartPairingRpc.Parser().parse(dataItems)
    }

    @Test(expected = IllegalArgumentException::class)
    fun testThrowsOnNullValueForPeerId() {
        // { "peer_id" : null }
        val dataItems = listOf(Map().put(UnicodeString(peerIdParam), SimpleValue.NULL))
        StartPairingRpc.Parser().parse(dataItems)
    }

    @Test(expected = IllegalArgumentException::class)
    fun testThrowsOnNullValueForIdField() {
        // { "peer_id" : {id: null, type: "macAddress"} }
        val peerIdItem = Map().put(UnicodeString(idParam), SimpleValue.NULL).put(UnicodeString(typeParam), UnicodeString(PeerIdType.MAC_ADDRESS.value))
        val dataItems = listOf(Map().put(UnicodeString(peerIdParam), peerIdItem))
        StartPairingRpc.Parser().parse(dataItems)
    }

    @Test(expected = IllegalArgumentException::class)
    fun testThrowsOnInvalidPeerType() {
        // { "peer_id" : {id: "AA:BB:CC:DD:EE:FF", type: "banana"} }
        val peerIdItem = Map().put(UnicodeString(idParam), UnicodeString(MAC_ADDRESS))
            .put(UnicodeString(typeParam), UnicodeString("banana"))
        val dataItems = listOf(Map().put(UnicodeString(peerIdParam), peerIdItem))
        StartPairingRpc.Parser().parse(dataItems)
    }

    @Test(expected = IllegalArgumentException::class)
    fun testThrowsOnInvalidPeerTypeType() {
        // { "peer_id" : {id: "AA:BB:CC:DD:EE:FF", type: 9} }
        val peerIdItem = Map().put(UnicodeString(idParam), UnicodeString(MAC_ADDRESS))
            .put(UnicodeString(typeParam), UnsignedInteger(9))
        val dataItems = listOf(Map().put(UnicodeString(peerIdParam), peerIdItem))
        StartPairingRpc.Parser().parse(dataItems)
    }

    @Test(expected = IllegalArgumentException::class)
    fun testThrowsOnInvalidPeerIdType() {
        // { "peer_id" : {id: 5, type: "macAddress"} }
        val peerIdItem = Map().put(UnicodeString(idParam), UnsignedInteger(5))
            .put(UnicodeString(typeParam), UnicodeString(PeerIdType.MAC_ADDRESS.value))
        val dataItems = listOf(Map().put(UnicodeString(peerIdParam), peerIdItem))
        StartPairingRpc.Parser().parse(dataItems)
    }

}
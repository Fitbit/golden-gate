package com.fitbit.remoteapi

import co.nstant.`in`.cbor.CborBuilder
import co.nstant.`in`.cbor.model.Map
import co.nstant.`in`.cbor.model.SimpleValue
import co.nstant.`in`.cbor.model.UnicodeString
import com.fitbit.remoteapi.handlers.SetTlsKeyRpc
import org.junit.Test
import org.junit.runner.RunWith
import org.junit.runners.JUnit4
import co.nstant.`in`.cbor.model.DataItem
import java.util.Collections
import kotlin.test.assertEquals

@RunWith(JUnit4::class)
class SetTlsKeyRpcHandlerTest {

    private val nameParam = "keyinfo"

    @Test(expected = IllegalArgumentException::class)
    fun testThrowsOnNullParams() {
        SetTlsKeyRpc.Parser().parse(Collections.emptyList())
    }

    @Test(expected = IllegalArgumentException::class)
    fun testThrowsOnNullValue() {
        // { "key_info" : null }
        val dataItems = listOf(Map().put(UnicodeString(nameParam), SimpleValue.NULL))
        SetTlsKeyRpc.Parser().parse(dataItems)
    }

    @Test(expected = IllegalArgumentException::class)
    fun testThrowsOnInvalidKeyId() {
        // { "key_info" : { "key_identity" : null, "key" : null }}
        val dataItems = CborBuilder()
                .addMap()
                .putMap(UnicodeString(nameParam))
                .put(UnicodeString("key_identity"), SimpleValue.NULL)
                .put(UnicodeString("key"), SimpleValue.NULL)
                .end()
                .end()
                .build()

        SetTlsKeyRpc.Parser().parse(dataItems as  List<DataItem>)
    }

    @Test
    fun testCommandParsing() {
        // { "key_info" : { "key_identity" : "hello", "key" : "0102030405060708090AF1F2F3F4F5F6" }}
        val keyIdData = "keyinfo"
        val keyData = "0102030405060708090AF1F2F3F4F5F6"
        val dataItems = CborBuilder()
                .addMap()
                .putMap(UnicodeString(nameParam))
                .put(UnicodeString("key_identity"), UnicodeString(keyIdData))
                .put(UnicodeString("key"), UnicodeString(keyData))
                .end()
                .end()
                .build()

        val result = SetTlsKeyRpc.Parser().parse(dataItems)
        assertEquals(Pair(keyIdData, keyData), result)
    }
}

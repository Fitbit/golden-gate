package com.fitbit.goldengate.bindings.io

import com.fitbit.goldengate.bindings.BaseTest
import com.nhaarman.mockitokotlin2.spy
import org.junit.After
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertNotNull
import org.junit.Before
import org.junit.Test
import java.util.Arrays

class TxSinkTest : BaseTest() {

    private lateinit var txSink: TxSink

    @Before
    fun setup() {
        txSink = spy(TxSink())
        assertNotNull(txSink)
    }

    @After
    fun tearDown() {
        txSink.close()
    }

    @Test
    fun assertTxCreated() {
        assertNotEquals(0, txSink.thisPointer)
    }

    @Test
    fun testPutData() {
        val bytes = ByteArray(5)
        txSink.putData(bytes)
        txSink.dataObservable
                .test()
                .assertValue { Arrays.equals(bytes, it) }
    }
}

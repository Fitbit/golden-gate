// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bindings.io

import com.fitbit.goldengate.bindings.BaseTest
import java.util.Arrays
import org.junit.After
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertNotNull
import org.junit.Before
import org.junit.Test
import org.mockito.kotlin.spy

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
    txSink.dataObservable.test().assertValue { Arrays.equals(bytes, it) }
  }
}

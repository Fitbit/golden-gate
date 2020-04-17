// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bindings

import org.junit.Assert
import org.junit.Test

class UtilTest : BaseTest() {

    @Test
    fun testSetupBlasterJNI() {
        Assert.assertEquals(0, JNITestHarness.runJNITest("Blaster.Setup"))
    }

    @Test
    fun testSetupPerfSink() {
        Assert.assertEquals(0, JNITestHarness.runJNITest("PerfDataSink.Setup"))
    }

    @Test
    fun testGetSinkStatsJNI() {
        Assert.assertEquals(0, JNITestHarness.runJNITest("PerfDataSink.GetStats"))
    }

    @Test
    fun testByteArrayToBufferJNI() {
        Assert.assertEquals(0, JNITestHarness.runJNITest("ByteArray.ToBuffer"))
    }

    @Test
    fun testBufferToByteArrayJNI() {
        Assert.assertEquals(0, JNITestHarness.runJNITest("Buffer.ToByteArray"))
    }
}

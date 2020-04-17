// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bindings

import org.junit.Assert
import org.junit.Assert.assertNotNull
import org.junit.Assert.fail
import org.junit.Test

class GoldenGateTest {
    @Test
    fun initializeNoParams() {
        try {
            GoldenGate.init()
            fail()
        } catch (e: UnsatisfiedLinkError) {
            //We expect this because the library isn't in the unit test's path
            assertNotNull(e.message)
        }
    }

    @Test
    fun testGetVersion() {
        /* this is a bit weird, but I just wanted to call the GoldenGate.init with path
         * inside base test, but this class cannot inherit from base test
         * otherwise the other test won't work.
         */
        BaseTest.setup()
        val version = GoldenGate.getVersion()
        assertNotNull(version)
    }

    @Test
    fun testInitializeJNI() {
        BaseTest.setup()
        Assert.assertEquals(0, JNITestHarness.runJNITest("GG_Module.Initialize"))
    }

    @Test
    fun testGetVersionJNI() {
        BaseTest.setup()
        Assert.assertEquals(0, JNITestHarness.runJNITest("GG_Module.GetVersion"))
    }

}

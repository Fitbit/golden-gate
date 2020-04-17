// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bindings

import androidx.annotation.Keep
import java.util.concurrent.CountDownLatch

internal class RunLoop : Thread() {

    val latch: CountDownLatch

    init {
        GoldenGate.check()

        latch = CountDownLatch(1)
    }

    //make sure this is running before we use it.
    override fun run() {
        startLoopJNI() //When this returns we're done with the loop, so destroy it.
        destroyLoopJNI()
    }

    fun stopLoop() {
        stopLoopJNI()
    }

    @Keep
    private fun onLoopCreated() {
        latch.countDown()
    }

    fun waitUntilReady() {
        latch.await()
    }

    fun startAndWaitUntilReady() {
        start()
        waitUntilReady()
    }

    private external fun startLoopJNI(clazz: Class<RunLoop> = RunLoop::class.java)
    private external fun stopLoopJNI()
    private external fun destroyLoopJNI()
}

// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bindings

class JNITestHarness {

    init {
        GoldenGate.check()
    }

    companion object {

        fun runJNITest(args: String) = runTestsJNI(args)

        @JvmStatic
        private external fun runTestsJNI(args: String) : Int
    }
}

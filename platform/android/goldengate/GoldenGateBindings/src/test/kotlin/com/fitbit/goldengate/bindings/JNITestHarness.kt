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

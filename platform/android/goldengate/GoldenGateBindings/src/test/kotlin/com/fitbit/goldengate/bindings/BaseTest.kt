// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bindings

import org.junit.AfterClass
import org.junit.BeforeClass
import java.io.File
import java.util.ArrayList

open class BaseTest {
    companion object {

        val searchPaths : List<String>
        val optionalPrefix = "GoldenGateBindings/"
        val bindingsPrefix = "../../../bindings/GoldenGate/"
        val infix = ".externalNativeBuild/cmake/debug/unix/libxp"
        val suffixes = arrayOf(".so", ".dylib")

        init {
            searchPaths = ArrayList<String>(6)
            searchPaths.add(infix + suffixes[0])
            searchPaths.add(infix + suffixes[1])
            searchPaths.add(optionalPrefix + infix + suffixes[0])
            searchPaths.add(optionalPrefix + infix + suffixes[1])
            searchPaths.add(bindingsPrefix + optionalPrefix + infix + suffixes[0])
            searchPaths.add(bindingsPrefix + optionalPrefix + infix + suffixes[1])
        }

        @BeforeClass
        @JvmStatic fun setup() {
            val path = searchPaths.map { File(it) }.filter { it.exists() }.first()
            GoldenGate.init(true, path)
        }

        @AfterClass
        @JvmStatic fun teardown() {
        }
    }
}

// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bindings

import android.annotation.SuppressLint
import com.fitbit.goldengate.bindings.logging.Logger
import com.fitbit.goldengate.bindings.logging.SimpleLogger
import timber.log.Timber
import java.io.File
import java.util.concurrent.atomic.AtomicBoolean

/**
 * This class holds general properties of the Golden Gate library like the logger, the GG Loop thread, etc.
 */
class GoldenGate {
    companion object {
        private val initialized = AtomicBoolean()

        private var logger: Logger? = null

        private lateinit var ggLoop : RunLoop

        /**
         * Pass a file if you would like the library it represents loaded. Otherwise, it will load the default "xp" library from the library path.
         */
        @JvmStatic
        @JvmOverloads
        @SuppressLint("UnsafeDynamicallyLoadedCode")
        fun init(loggingEnabled: Boolean = true, libPath: File? = null) {
            if (libPath != null) {
                System.load(libPath.absolutePath)
            } else {
                //This will throw when run in the Unit Test JVM, so the Jacoco doesn't see this path as being covered.
                System.loadLibrary("xp")
            }
            if (initialized.getAndSet(true)) {
                Timber.d("GoldenGate XP module already initialized")
                return
            }
            Timber.d("Initializing GoldenGate XP module")
            initModulesJNI()
            registerLoggerJNI() //This registers the C++ log handler with the GG library
            if (loggingEnabled) {
                logger = SimpleLogger() //This constructor actually hooks logs from JNI and XP code to Timber.
            }
            ggLoop = RunLoop()
            ggLoop.startAndWaitUntilReady()
        }

        fun deInit() {
            ggLoop.stopLoop()
            logger?.close()
            initialized.set(false)
        }

        /**
         * Get the version information from the Golden Gate library
         *
         * @return the version information for this library
         */
        fun getVersion() : GoldenGate.Version {
            return getVersionJNI()
        }

        /**
         * Ping Golden Gate library. This will make a gg loop invoke sync call, which will wake up the gg loop’s select
         * and then return an incremental counter value
         *
         * @return a counter value
         */
        fun ping(): Int {
            return pingJNI()
        }

        /* this method is neccesary to inject the our console logging callback into the Golden Gate code */
        @JvmStatic
        private external fun registerLoggerJNI()

        @JvmStatic
        private external fun initModulesJNI()

        @JvmStatic
        private external fun getVersionJNI(clazz: Class<Version> = Version::class.java) : Version

        @JvmStatic
        private external fun pingJNI(): Int

        /**
         * Verify that the Golden Gate C library has been loaded.
         *
         * @throws IllegalStateException if the library has not been loaded
         */
        @JvmStatic fun check() {
            if (!initialized.get()) {
                throw IllegalStateException("You must call GoldenGate.init() before calling this method")
            }
        }
    }

    /**
     * This class represents the version information for the GG Library
     *
     * @property maj Major verison number
     * @property min Minor version number
     * @property patch Patch version number
     * @property commitCount How many commits there have been since the version tag corresponding to [maj].[min].[patch]
     * @property commitHash The git hash of HEAD when this library was built.
     * @property branchName the git branch name this library was built from
     * @property buildDate the date this library was built
     * @property buildTime the time this library was built
     */
    data class Version(val maj: Long, val min: Long, val patch: Long, val commitCount: Int,
                       val commitHash: String, val branchName: String,
                       val buildDate: String, val buildTime: String)
}

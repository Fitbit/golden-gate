// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bindings.logging

import android.util.Log
import com.fitbit.goldengate.bindings.BaseTest
import org.junit.Assert.assertEquals
import org.junit.Test

class LoggerTest : BaseTest() {

    /* From gg_logging.h:
        #define GG_LOG_LEVEL_FATAL   700
        #define GG_LOG_LEVEL_SEVERE  600
        #define GG_LOG_LEVEL_WARNING 500
        #define GG_LOG_LEVEL_INFO    400
        #define GG_LOG_LEVEL_FINE    300
        #define GG_LOG_LEVEL_FINER   200
        #define GG_LOG_LEVEL_FINEST  100
     */
    @Test
    fun testStandardLogLevelParsing() {
        assertEquals(LogLevel.FATAL, LogLevel.getLevel(700))
        assertEquals(Log.ERROR, LogLevel.FATAL.priority)

        assertEquals(LogLevel.SEVERE, LogLevel.getLevel(600))
        assertEquals(Log.ERROR, LogLevel.SEVERE.priority)

        assertEquals(LogLevel.WARNING, LogLevel.getLevel(500))
        assertEquals(Log.WARN, LogLevel.WARNING.priority)

        assertEquals(LogLevel.INFO, LogLevel.getLevel(400))
        assertEquals(Log.INFO, LogLevel.INFO.priority)

        assertEquals(LogLevel.FINE, LogLevel.getLevel(300))
        assertEquals(Log.DEBUG, LogLevel.FINE.priority)

        assertEquals(LogLevel.FINER, LogLevel.getLevel(200))
        assertEquals(Log.VERBOSE, LogLevel.FINER.priority)

        assertEquals(LogLevel.FINEST, LogLevel.getLevel(100))
        assertEquals(Log.VERBOSE, LogLevel.FINEST.priority)
    }

    @Test
    fun testNonStandardLogLevelParsing() {
        assertEquals(LogLevel.FATAL, LogLevel.getLevel(20000))
        assertEquals(LogLevel.SEVERE, LogLevel.getLevel(652))
        assertEquals(LogLevel.FINEST, LogLevel.getLevel(7))
        assertEquals(LogLevel.FINEST, LogLevel.getLevel(147))
    }
}

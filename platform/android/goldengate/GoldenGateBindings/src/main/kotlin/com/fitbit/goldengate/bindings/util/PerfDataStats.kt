// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bindings.util

data class PerfDataStats(val packetsReceived: Long, val bytesReceived: Long, val throughput: Long,
                    val lastReceivedCounter: Long, val nextExpectedCounter: Long,
                    val gapCount: Long, val passthroughWouldBlockCount: Long) {
    override fun toString(): String {
        return (
                "Packets received: $packetsReceived," +
                        "bytes received: $bytesReceived," +
                        "throughput: $throughput bytes/s," +
                        "Last received counter: $lastReceivedCounter," +
                        "Next expected counter: $nextExpectedCounter," +
                        "Gap count : $gapCount," +
                        "Pass through would block count: $passthroughWouldBlockCount,"
                )
    }
}

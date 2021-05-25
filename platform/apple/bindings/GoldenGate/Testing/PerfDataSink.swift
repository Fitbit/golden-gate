//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  PerfDataSink.swift
//  GoldenGate
//
//  Created by Bogdan Vlad on 1/15/18.
//

import GoldenGateXP

public enum PerfDataSinkMode: UInt32 {
    case raw
    case basicOrIP
}

public struct TrafficPerformanceStats {
    /// Total number of packets received
    public var packetsReceived: Int

    /// Total number of bytes received
    public var bytesReceived: Int

    /// Throughput in bytes/s
    public var throughput: UInt32

    /// Last packet counter received
    public var lastReceivedCounter: UInt32

    /// Expected next counter
    public var nextExpectedCounter: UInt32

    /// Number of detected counter gaps
    public var gapCount: Int

    // Number of times the passthrough sink
    public var passthroughWouldBlockCount: Int
}

extension TrafficPerformanceStats {
    init() {
        self.init(
            packetsReceived: 0,
            bytesReceived: 0,
            throughput: 0,
            lastReceivedCounter: 0,
            nextExpectedCounter: 0,
            gapCount: 0,
            passthroughWouldBlockCount: 0
        )
    }

    init(rawValue: GG_PerfDataSinkStats) {
        self.init(
            packetsReceived: rawValue.packets_received,
            bytesReceived: rawValue.bytes_received,
            throughput: rawValue.throughput,
            lastReceivedCounter: rawValue.last_received_counter,
            nextExpectedCounter: rawValue.next_expected_counter,
            gapCount: rawValue.gap_count,
            passthroughWouldBlockCount: rawValue.passthrough_would_block_count
        )
    }
}

extension TrafficPerformanceStats: CustomStringConvertible {
    public var description: String {
        let throughput = String(format: "%.1f", Double(self.throughput) / 1000)

        return [
            "thrpt=\(throughput)",
            "bytes=\(bytesReceived)",
            "packets=\(packetsReceived)",
            "counter=(" + [
                "last=\(lastReceivedCounter)",
                "next=\(nextExpectedCounter)",
                "gaps=\(gapCount)",
                "wouldblock=\(passthroughWouldBlockCount)"
            ].joined(separator: ",") + ")"
        ].joined(separator: " ")
    }
}

public struct PerfDataSinkOption: OptionSet {
    public let rawValue: UInt32

    /// When this flag is set in the options, the stats will be printed on the console
    public static let printStatsToConsole = PerfDataSinkOption(rawValue: UInt32(GG_PERF_DATA_SINK_OPTION_PRINT_STATS_TO_CONSOLE))

    /// When this flag is set in the options, the stats will be logged with level INFO
    public static let printStatsToLog = PerfDataSinkOption(rawValue: UInt32(GG_PERF_DATA_SINK_OPTION_PRINT_STATS_TO_LOG))

    public init(rawValue: UInt32) {
        self.rawValue = rawValue
    }
}

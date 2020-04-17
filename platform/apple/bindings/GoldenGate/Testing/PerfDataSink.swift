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

/// Data sink that can be used for performance measurements.
public class PerfDataSink: DataSource, DataSink {
    typealias Ref = OpaquePointer

    private let ref: Ref
    private let runLoop: GoldenGate.RunLoop
    private var passthroughTarget: ManagedDataSink?

    public init(
        runLoop: GoldenGate.RunLoop,
        mode: PerfDataSinkMode = .basicOrIP,
        options: PerfDataSinkOption = .printStatsToLog,
        statsPrintTimeInterval: UInt32 = 5
    ) throws {
        var ref: Ref?
        try GG_PerfDataSink_Create(
            GG_PerfDataSinkMode(mode.rawValue),
            options.rawValue,
            statsPrintTimeInterval,
            &ref
        ).rethrow()
        self.ref = ref!

        self.runLoop = runLoop
    }

    deinit {
        runLoop.async { [ref] in
            GG_PerfDataSink_Destroy(ref)
        }
    }

    /// The last stats recorded by the performance data sink.
    public var stats: TrafficPerformanceStats {
        return TrafficPerformanceStats(
            rawValue: GG_PerfDataSink_GetStats(ref)!.pointee
        )
    }

    private lazy var dataSink: DataSink = {
        return UnmanagedDataSink(GG_PerfDataSink_AsDataSink(ref))
    }()

    public func put(_ buffer: Buffer, metadata: DataSink.Metadata?) throws {
        try dataSink.put(buffer, metadata: metadata)
    }

    public func setDataSinkListener(_ dataSinkListener: DataSinkListener?) throws {
        try dataSink.setDataSinkListener(dataSinkListener)
    }

    public func setDataSink(_ dataSink: DataSink?) throws {
        runLoopPrecondition(condition: .onRunLoop)
        self.passthroughTarget = dataSink.map(ManagedDataSink.init)
        GG_PerfDataSink_SetPassthroughTarget(ref, self.passthroughTarget?.ref)
    }

    /// Stats will be reset asynchronously on the RunLoop's thread.
    func resetStats() {
        runLoop.async { [ref] in
            GG_PerfDataSink_ResetStats(ref)
        }
    }
}

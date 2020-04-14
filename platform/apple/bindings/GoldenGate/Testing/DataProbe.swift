//
//  DataProbe.swift
//  GoldenGate
//
//  Created by Sylvain Rebaud on 3/29/19.
//  Copyright © 2019 Fitbit. All rights reserved.
//

import GoldenGateXP
import RxSwift

public protocol DataProbeReportProvider: class {
    var report: DataProbeReport { get }
}

public struct DataProbeReport {
    public var totalBytes: Int
    public var totalThroughput: UInt32
    public var totalThroughputPeak: UInt32
    public var windowThroughput: UInt32
    public var windowThroughputPeak: UInt32
    public var windowBytesSecond: UInt32
    public var windowBytesSecondPeak: UInt32
}

extension DataProbeReport {
    init() {
        self.init(
            totalBytes: 0,
            totalThroughput: 0,
            totalThroughputPeak: 0,
            windowThroughput: 0,
            windowThroughputPeak: 0,
            windowBytesSecond: 0,
            windowBytesSecondPeak: 0
        )
    }

    init(rawValue: GG_DataProbeReport) {
        self.init(
            totalBytes: rawValue.total_bytes,
            totalThroughput: rawValue.total_throughput,
            totalThroughputPeak: rawValue.total_throughput_peak,
            windowThroughput: rawValue.window_throughput,
            windowThroughputPeak: rawValue.window_throughput_peak,
            windowBytesSecond: rawValue.window_bytes_second,
            windowBytesSecondPeak: rawValue.window_bytes_second_peak
        )
    }
}

public struct DataProbeReportOption: OptionSet {
    public let rawValue: UInt32

    /// When this flag is set in the options, the stats will include total throughput calcultion
    public static let totalThroughput = DataProbeReportOption(rawValue: UInt32(GG_DATA_PROBE_OPTION_TOTAL_THROUGHPUT))

    /// When this flag is set in the options, the stats will include windowed throughput calcultion
    public static let windowThroughput = DataProbeReportOption(rawValue: UInt32(GG_DATA_PROBE_OPTION_WINDOW_THROUGHPUT))

    /// When this flag is set in the options, the stats will include window integral calcultion
    public static let windowIntegral = DataProbeReportOption(rawValue: UInt32(GG_DATA_PROBE_OPTION_WINDOW_INTEGRAL))

    public init(rawValue: UInt32) {
        self.rawValue = rawValue
    }
}

public class DataProbe {
    typealias Ref = OpaquePointer

    private let ref: Ref
    private let runLoop: GoldenGate.RunLoop
    private let options: DataProbeReportOption

    public init(
        runLoop: GoldenGate.RunLoop,
        options: DataProbeReportOption = .totalThroughput,
        bufferSize: UInt32 = 1000,
        windowSize: UInt32 = 1000,
        reportInterval: TimeInterval = 1
    ) throws {
        var ref: Ref?
        try GG_DataProbe_Create(
            options.rawValue,
            bufferSize,
            windowSize,
            0,
            nil,
            &ref
        ).rethrow()
        self.ref = ref!

        self.runLoop = runLoop
        self.options = options
    }

    deinit {
        runLoop.async { [ref] in
            GG_DataProbe_Destroy(ref)
        }
    }

    public func accumulate(count: Int) {
        runLoop.async { [ref] in
            GG_DataProbe_Accumulate(ref, count)
        }
    }

    public func reset() {
        runLoop.sync {
            GG_DataProbe_Reset(ref)
        }
    }

    /// Get report
    public var report: DataProbeReport {
        return runLoop.sync {
            DataProbeReport(
                rawValue: GG_DataProbe_GetReport(ref)!.pointee
            )
        }
    }
}

extension DataProbe: DataProbeReportProvider {}

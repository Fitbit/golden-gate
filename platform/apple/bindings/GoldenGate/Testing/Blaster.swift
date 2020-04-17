//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  Blaster.swift
//  GoldenGate
//
//  Created by Bogdan Vlad on 1/10/18.
//

import GoldenGateXP

public class Blaster: DataSource {
    public struct Configuration {
        let packetSize: Int
        let packetFormat: BlastPacketFormat
        let maxPacketCount: Int
        let interval: UInt32

        public init(
            packetSize: Int,
            packetFormat: BlastPacketFormat,
            maxPacketCount: Int,
            interval: UInt32
        ) {
            self.packetSize = packetSize
            self.packetFormat = packetFormat
            self.maxPacketCount = maxPacketCount
            self.interval = interval
        }

        public static let `default` = Configuration(
            packetSize: 30,
            packetFormat: .ip,
            maxPacketCount: 0,
            interval: 0
        )
    }

    typealias Ref = OpaquePointer

    private let runLoop: RunLoop
    private var ref: Ref
    private var dataSink: ManagedDataSink?

    public init(
        runLoop: RunLoop,
        configuration: Configuration
    ) {
        self.runLoop = runLoop

        var ref: Ref?
        GG_BlasterDataSource_Create(
            configuration.packetSize,
            configuration.packetFormat.gg,
            configuration.maxPacketCount,
            runLoop.timerSchedulerRef,
            configuration.interval,
            &ref
        )
        self.ref = ref!
    }

    public func setDataSink(_ dataSink: DataSink?) {
        runLoopPrecondition(condition: .onRunLoop)

        self.dataSink = dataSink.map(ManagedDataSink.init)

        GG_DataSource_SetDataSink(
            GG_BlasterDataSource_AsDataSource(ref),
            self.dataSink?.ref
        )
    }

    public func start() {
        runLoop.async {
            GG_BlasterDataSource_Start(self.ref)
        }
    }

    public func stop() {
        runLoop.async {
            GG_BlasterDataSource_Stop(self.ref)
        }
    }

    deinit {
        runLoop.async { [ref, dataSink] in
            withExtendedLifetime(dataSink) {
                GG_BlasterDataSource_Destroy(ref)
            }
        }
    }
}

public enum BlastPacketFormat: UInt32 {
    case counter
    // swiftlint:disable:next identifier_name
    case ip
}

extension BlastPacketFormat {
    var gg: GG_BlasterDataSourcePacketFormat {
        return GG_BlasterDataSourcePacketFormat(rawValue)
    }
}

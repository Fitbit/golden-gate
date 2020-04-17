//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  BlasterService.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 11/28/17.
//

import Foundation
import GoldenGateXP
import RxCocoa
import RxSwift

/// Service that blasts bytes on a socket to the other side
/// when it becomes available and `shouldBlast` is set.
public final class BlasterService {
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

    public let shouldBlast = BehaviorRelay(value: false)
    public let isBlasting: Observable<Bool>

    private var ref: Ref
    private let runLoop: GoldenGate.RunLoop
    private let port: Observable<Port?>
    private let disposeBag = DisposeBag()
    private var portDataSink: UnsafeDataSink?
    private var portDataSource: UnsafeDataSource?

    public init(runLoop: GoldenGate.RunLoop, configuration: Observable<Configuration>, port: Observable<Port?>) throws {
        runLoopPrecondition(condition: .onRunLoop)

        var ref: Ref?
        try GG_BlastService_Create(runLoop.ref, &ref).rethrow()
        self.ref = ref!

        let isBlastingRelay = BehaviorRelay(value: false)
        self.isBlasting = isBlastingRelay.asObservable()
        self.runLoop = runLoop

        self.port = port

        port
            .observeOn(runLoop)
            .do(onNext: { [weak self] port in
                guard let self = self else { return }

                let dataSink = port?.dataSink.gg
                let dataSource = port?.dataSource.gg

                try GG_BlastService_Attach(self.ref, dataSource?.ref, dataSink?.ref).rethrow()

                self.portDataSink = dataSink
                self.portDataSource = dataSource
            })
            .subscribe()
            .disposed(by: disposeBag)

        Observable
            .combineLatest(port, shouldBlast.distinctUntilChanged(), configuration)
            .observeOn(runLoop)
            .map { port, shouldBlast, configuration in
                return shouldBlast ? (port, configuration) : (nil, configuration)
            }
            .do(onNext: { port, configuration in
                GG_BlastService_StopBlaster(ref)

                if port != nil {
                    GG_BlastService_StartBlaster(
                        ref,
                        configuration.packetSize,
                        configuration.maxPacketCount,
                        Int(configuration.interval)
                    )
                }
            })
            .map { $0.0 != nil }
            .bind(to: isBlastingRelay)
            .disposed(by: disposeBag)
    }

    /// Resets the stats of the inbound traffic.
    public func resetStats() {
        GG_BlastService_ResetStats(ref)
    }

    /// The current stats.
    public var stats: Observable<TrafficPerformanceStats> {
        return Observable<Int64>.interval(.milliseconds(500), scheduler: MainScheduler())
            .map { _ in
                var stats = GG_PerfDataSinkStats()
                try GG_BlastService_GetStats(self.ref, &stats).rethrow()
                return TrafficPerformanceStats(rawValue: stats)
            }
    }

    deinit {
        runLoop.async { [ref, portDataSink] in
            _ = portDataSink

            try? GG_BlastService_Attach(ref, nil, nil).rethrow()

            GG_BlastService_Destroy(ref)
        }
    }
}

extension BlasterService: RemoteApiModule {
    public var methods: Set<String> { [] } // Methods are defined in xp

    public func publishHandlers(on remoteShell: RemoteShell) {
        GG_BlastService_Register(ref, remoteShell.ref)
    }

    public func unpublishHandlers(from remoteShell: RemoteShell) {
        GG_BlastService_Unregister(ref, remoteShell.ref)
    }
}

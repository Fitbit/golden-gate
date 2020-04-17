//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  TrafficAnalyzingLink.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 11/29/17.
//

import Foundation
import RxSwift

/// Observes the throughput on a link in both directions and emits.
class TrafficAnalyzingLink: NetworkLink {
    private let link: NetworkLink
    private let outgoingAnalyzer: ThroughputAnalyzer
    private let incomingAnalyzer: ThroughputAnalyzer

    let dataSource: DataSource
    let dataSink: DataSink

    var incomingStats: Observable<TrafficPerformanceStats?>
    var outgoingStats: Observable<TrafficPerformanceStats?>

    init(link: NetworkLink) throws {
        self.link = link

        self.outgoingAnalyzer = ThroughputAnalyzer()
        outgoingStats = outgoingAnalyzer.stats
        outgoingAnalyzer.setDataSink(link.dataSink)

        self.incomingAnalyzer = ThroughputAnalyzer()
        incomingStats = incomingAnalyzer.stats
        try link.dataSource.setDataSink(incomingAnalyzer)

        dataSink = outgoingAnalyzer
        dataSource = incomingAnalyzer
    }

    func resetStats() {
        outgoingAnalyzer.resetStats()
        incomingAnalyzer.resetStats()
    }
}

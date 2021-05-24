//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  NetworkLink.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 2/23/18.
//

import Foundation
import RxRelay

/// Used to create network interfaces.
public struct NetworkLink: Port {
    public let dataSink: DataSink
    public let dataSource: DataSource
    public let mtu: BehaviorRelay<UInt>

    public init(dataSink: DataSink, dataSource: DataSource, mtu: BehaviorRelay<UInt>) {
        self.dataSink = dataSink
        self.dataSource = dataSource
        self.mtu = mtu
    }
}

extension NetworkLink: CustomStringConvertible {
    public var description: String {
        return "NetworkLink mtu=\(mtu.value)"
    }
}

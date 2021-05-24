//
//  NetworkLink.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 2/23/18.
//  Copyright © 2018 Fitbit. All rights reserved.
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

//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  MockConnection.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 12/8/17.
//

import Foundation
import GoldenGate
import RxCocoa
import RxSwift

class MockConnection: Connection {
    private let disposeBag = DisposeBag()

    let networkLinkStatus = BehaviorRelay(value: NetworkLinkStatus.disconnected)

    init() {
        // empty variant
    }

    init(networkLinkStatus: Observable<NetworkLinkStatus>) {
        networkLinkStatus.subscribe(onNext: self.networkLinkStatus.accept).disposed(by: disposeBag)
    }

    var description: String {
        return "\(type(of: self))"
    }
}

class MockLink: NetworkLink {
    var mtu = BehaviorRelay<UInt>(value: 1152)
    let dataSink: DataSink = VoidDataSink.instance
    let dataSource: DataSource = MockDataSource()
}

//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  MockConnection.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 12/8/17.
//

import BluetoothConnection
import Foundation
import RxCocoa
import RxSwift

class MockConnection: Connection {
    var descriptor: PeerDescriptor

    private let disposeBag = DisposeBag()

    let networkLink = BehaviorRelay<NetworkLink?>(value: nil)

    init() {
        // empty variant
        self.descriptor = PeerDescriptor(identifier: UUID())
    }

    convenience init(networkLink: Observable<NetworkLink>) {
        self.init()
        networkLink.subscribe(onNext: self.networkLink.accept).disposed(by: disposeBag)
    }

    var description: String {
        return "\(type(of: self))"
    }
}

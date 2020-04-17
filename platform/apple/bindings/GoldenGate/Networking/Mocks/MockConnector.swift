//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  MockConnector.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 12/8/17.
//

import Foundation
import GoldenGate
import RxSwift

struct MockConnector: Connector {
    typealias Descriptor = String

    let connection: Observable<Connection>

    func establishConnection(with descriptor: String) -> Observable<Connection> {
        return connection
    }
}

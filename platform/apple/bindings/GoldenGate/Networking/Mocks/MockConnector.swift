//
//  MockConnector.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 12/8/17.
//  Copyright © 2017 Fitbit. All rights reserved.
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

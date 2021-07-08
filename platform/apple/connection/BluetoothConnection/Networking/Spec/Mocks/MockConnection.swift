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
import RxSwift

final class MockConnection: Connection {
    var descriptor: PeerDescriptor
    var modelNumber = Observable<String>.never()
    var serialNumber = Observable<String>.never()
    var firmwareRevision = Observable<String>.never()
    var hardwareRevision = Observable<String>.never()

    init() {
        self.descriptor = PeerDescriptor(identifier: UUID())
    }
}

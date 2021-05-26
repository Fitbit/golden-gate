//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  MockConnector.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 12/8/17.
//

import BluetoothConnection
import Foundation
import RxSwift

final class MockConnector: Connector {
    var connectionStatus = PublishSubject<ConnectionStatus<Connection>>()

    func establishConnection(descriptor: PeerDescriptor) -> Observable<ConnectionStatus<Connection>> {
        connectionStatus.asObservable()
    }
}

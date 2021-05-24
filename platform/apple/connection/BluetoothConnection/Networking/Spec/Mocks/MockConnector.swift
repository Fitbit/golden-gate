//
//  MockConnector.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 12/8/17.
//  Copyright © 2017 Fitbit. All rights reserved.
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

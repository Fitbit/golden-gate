//
//  MockConnectionController.swift
//  BluetoothConnectionTests
//
//  Created by Emanuel Jarnea on 10.02.2022.
//  Copyright © 2022 Fitbit. All rights reserved.
//

import BluetoothConnection
import Foundation
import RxRelay
import RxSwift

final class MockConnectionController: ConnectionControllerType, Instrumentable, CustomStringConvertible {
    let descriptorRelay = BehaviorRelay<PeerDescriptor?>(value: nil)
    var descriptor: Observable<PeerDescriptor?> { descriptorRelay.asObservable() }

    let connectionStatusSubject = ReplaySubject<ConnectionStatus<MockConnection>>.create(bufferSize: 1)
    var connectionStatus: Observable<ConnectionStatus<MockConnection>> { connectionStatusSubject.asObservable() }

    let connectionErrorSubject = ReplaySubject<Error?>.create(bufferSize: 1)
    var connectionError: Observable<Error?> { connectionErrorSubject.asObservable() }

    let metricsSubject = PublishSubject<ConnectionControllerMetricsEvent>()
    var metrics: Observable<ConnectionControllerMetricsEvent> { metricsSubject.asObservable() }

    private(set) var lastDescriptor: PeerDescriptor?
    func clearDescriptor() {
        lastDescriptor = nil
    }

    func update(descriptor: PeerDescriptor) {
        lastDescriptor = descriptor
    }

    private(set) var lastConnectTrigger: ConnectionTrigger?
    func establishConnection(trigger: ConnectionTrigger) {
        lastConnectTrigger = trigger
    }

    private(set) var lastDisconnectTrigger: ConnectionTrigger?
    func disconnect(trigger: ConnectionTrigger) {
        lastDisconnectTrigger = trigger
    }

    let description = "SomeDescription"
}

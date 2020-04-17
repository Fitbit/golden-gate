//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  MockConnectionController.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 12/8/17.
//

import Foundation
import GoldenGate
import RxCocoa
import RxSwift

class MockConnectionController: ConnectionControllerType {
    typealias Connector = MockConnector
    typealias Descriptor = String
    typealias Stack = MockStack

    let sharedConnection: Completable

    let stateRelay: BehaviorRelay<ConnectionControllerState>
    var state: Observable<ConnectionControllerState> { return stateRelay.asObservable() }

    let descriptor: Observable<String?>

    let connectionRelay: BehaviorRelay<Connection?>
    var connection: Observable<Connection?> { return connectionRelay.asObservable() }

    let linkRelay: BehaviorRelay<NetworkLink?>
    var link: Observable<NetworkLink?> { return linkRelay.asObservable() }

    let stack = Observable<MockStack?>.just(nil)

    private let didForceDisconnectSubject = PublishSubject<Void>()
    var didForceDisconnect: Observable<Void> {
        return didForceDisconnectSubject.asObservable()
    }

    init(
        sharedConnection: (BehaviorRelay<ConnectionControllerState>) -> Completable,
        state: BehaviorRelay<ConnectionControllerState>? = nil,
        descriptor: Observable<String?>? = nil,
        connection: BehaviorRelay<Connection?>? = nil,
        link: BehaviorRelay<NetworkLink?>? = nil
    ) {
        self.stateRelay = state ?? .init(value: .disconnected)
        self.sharedConnection = sharedConnection(self.stateRelay).asObservable().share().asCompletable()
        self.descriptor = descriptor ?? Observable.never().startWith(nil)
        self.connectionRelay = connection ?? .init(value: nil)
        self.linkRelay = link ?? .init(value: nil)
    }

    func establishConnection() -> Completable {
        return sharedConnection
            .asObservable()
            .takeUntil(didForceDisconnectSubject.asObservable())
            .ignoreElements()
    }

    func update(descriptor: String) {

    }

    func clearDescriptor() {
        forceDisconnect()
    }

    func forceDisconnect() {
        didForceDisconnectSubject.on(.next(()))
    }
}

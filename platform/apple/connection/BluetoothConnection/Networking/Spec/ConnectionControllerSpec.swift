//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  ConnectionControllerSpec.swift
//  BluetoothConnection
//
//  Created by Marcel Jackwerth on 12/6/17.
//

import BluetoothConnection
import CoreBluetooth
import Nimble
import Quick
import RxRelay
import RxSwift
import RxTest

// swiftlint:disable file_length

private final class MockReconnectStrategy: ReconnectStrategy {
    let resetFailureHistory = PublishRelay<Void>()

    var stagedAction: ((Error) -> ReconnectStrategyAction)!
    func action(for error: Error) -> ReconnectStrategyAction { stagedAction(error) }
}

final class ConnectionControllerSpec: QuickSpec {
    // swiftlint:disable:next function_body_length
    override func spec() {
        let descriptor = PeerDescriptor(identifier: UUID(uuidString: "6B66A7A9-C4F3-4C2A-811A-79EFB7B8A85F")!)
        let anotherDescriptor = PeerDescriptor(identifier: UUID(uuidString: "15e35044-8f44-4ae8-b949-b1a019ea35c5")!)
        let someConnection = MockConnection()
        let trigger: ConnectionTrigger = "TEST"

        var connector: MockConnector!
        var reconnectStrategy: MockReconnectStrategy!
        var scheduler: TestScheduler!
        var disposeBag: DisposeBag!

        var controller: ConnectionController<Connection>!

        beforeEach {
            connector = MockConnector()
            reconnectStrategy = MockReconnectStrategy()
            reconnectStrategy.stagedAction = { .stop($0) }
            scheduler = TestScheduler(initialClock: 0, simulateProcessingDelay: false)
            disposeBag = DisposeBag()

            controller = ConnectionController(
                connector: connector,
                descriptor: descriptor,
                reconnectStrategy: reconnectStrategy,
                scheduler: scheduler,
                debugIdentifier: "test"
            )
        }

        describe("lifecycle") {
            it("deallocates") {
                controller.establishConnection(trigger: trigger) ~~> scheduler

                weak var controllerReference = controller

                controller = nil
                disposeBag = nil

                expect(controllerReference).to(beNil())
            }
        }

        context("descriptor") {
            it("changes the descriptor") {
                var events: [PeerDescriptor?] = []

                controller.descriptor
                    .subscribe(onNext: { events.append($0) })
                    .disposed(by: disposeBag)

                controller.update(descriptor: anotherDescriptor) ~~> scheduler

                expect(events) == [descriptor, anotherDescriptor]
            }

            it("clears the descriptor") {
                var events: [PeerDescriptor?] = []

                controller.descriptor
                    .subscribe(onNext: { events.append($0) })
                    .disposed(by: disposeBag)

                controller.clearDescriptor() ~~> scheduler

                expect(events) == [descriptor, nil]
            }

            it("disconnects on nil descriptor & reconnects on non nil descriptor") {
                var events: [ConnectionStatus<Connection>] = []

                controller.connectionStatus
                    .subscribe(onNext: { events.append($0) })
                    .disposed(by: disposeBag)

                controller.establishConnection(trigger: trigger) ~~> scheduler

                connector.connectionStatus.onNext(.connecting)

                controller.clearDescriptor()

                controller.update(descriptor: descriptor) ~~> scheduler

                connector.connectionStatus.onNext(.connecting)

                expect(events) == [.disconnected, .connecting, .disconnected, .connecting]
            }

            it("reconnects on new descriptor") {
                var events: [ConnectionStatus<Connection>] = []

                controller.connectionStatus
                    .subscribe(onNext: { events.append($0) })
                    .disposed(by: disposeBag)

                controller.establishConnection(trigger: trigger) ~~> scheduler

                connector.connectionStatus.onNext(.connected(someConnection))

                controller.update(descriptor: anotherDescriptor) ~~> scheduler

                expect(events) == [.disconnected, .connecting, .connected(someConnection), .connecting]
            }

            it("doesn't connect on new descriptor if connection not requested") {
                var events: [ConnectionStatus<Connection>] = []

                controller.connectionStatus
                    .subscribe(onNext: { events.append($0) })
                    .disposed(by: disposeBag)

                controller.update(descriptor: anotherDescriptor) ~~> scheduler

                connector.connectionStatus.onNext(.connecting)

                expect(events) == [.disconnected]
            }

            it("clears the descriptor if unknown for iOS & disconnects") {
                var events: [ConnectionStatus<Connection>] = []

                controller.connectionStatus
                    .subscribe(onNext: { events.append($0) })
                    .disposed(by: disposeBag)

                controller.establishConnection(trigger: trigger) ~~> scheduler

                var descriptors: [PeerDescriptor?] = []

                controller.descriptor
                        .subscribe(onNext: { descriptors.append($0) })
                        .disposed(by: disposeBag)

                connector.connectionStatus.onNext(.connecting)
                connector.connectionStatus.onError(CentralManagerError.identifierUnknown)

                expect(descriptors) == [descriptor, nil]
                expect(events) == [.disconnected, .connecting, .disconnected]
            }
        }

        describe("status") {
            it("emits connection status & errors correctly") {
                var statuses: [ConnectionStatus<Connection>] = []

                controller.connectionStatus
                    .subscribe(onNext: { statuses.append($0) })
                    .disposed(by: disposeBag)

                var errors: [Error] = []

                controller.connectionErrors
                    .subscribe(onNext: { errors.append($0) })
                    .disposed(by: disposeBag)

                controller.establishConnection(trigger: trigger) ~~> scheduler

                let reconnectTrigger = PublishSubject<Void>()
                reconnectStrategy.stagedAction = { error in
                    switch error {
                    case ConnectorError.disconnected: return .suspendUntil(reconnectTrigger)
                    default: return .stop(error)
                    }
                }

                connector.connectionStatus.onNext(.connecting)
                connector.connectionStatus.onNext(.disconnected)

                reconnectTrigger.onNext(()) ~~> scheduler

                connector.connectionStatus.onNext(.connecting)
                connector.connectionStatus.onNext(.connected(someConnection))
                connector.connectionStatus.onError(TestError.someError)

                expect(statuses) == [.disconnected, .connecting, .disconnected, .connecting, .connected(someConnection), .disconnected]
                expect(errors.count) == 2
                var iterator = errors.makeIterator()
                expect(iterator.next()).to(matchError(ConnectorError.disconnected))
                expect(iterator.next()).to(matchError(TestError.someError))
            }

            it("emits half bonded state") {
                guard #available(iOS 13.4, macOS 10.15, *) else { return }

                var events: [Bool] = []

                controller.isHalfBonded
                    .subscribe(onNext: { events.append($0) })
                    .disposed(by: disposeBag)

                controller.establishConnection(trigger: trigger) ~~> scheduler

                let reconnectTrigger = PublishSubject<Void>()
                reconnectStrategy.stagedAction = { _ in .suspendUntil(reconnectTrigger) }

                connector.connectionStatus.onNext(.connected(someConnection))

                connector.connectionStatus.onError(NSError(domain: CBErrorDomain, code: CBError.peerRemovedPairingInformation.rawValue, userInfo: nil))

                // Reset the old subject, otherwise it would re-emit the error
                connector.connectionStatus = PublishSubject()

                reconnectTrigger.onNext(()) ~~> scheduler

                connector.connectionStatus.onNext(.connected(someConnection))

                expect(events) == [false, true, false]
            }
        }

        describe("connection establisher") {
            it("multiple establishConnection(trigger: trigger) calls tigger a single connection attempt") {
                var events: [ConnectionStatus<Connection>] = []

                controller.connectionStatus
                    .subscribe(onNext: { events.append($0) })
                    .disposed(by: disposeBag)

                controller.establishConnection(trigger: trigger) ~~> scheduler

                connector.connectionStatus.onNext(.connecting)

                controller.establishConnection(trigger: trigger) ~~> scheduler

                expect(events) == [.disconnected, .connecting]
            }

            it("disconnect() cancels the ongoing connection & disables reconnects") {
                var events: [ConnectionStatus<Connection>] = []

                controller.connectionStatus
                    .subscribe(onNext: { events.append($0) })
                    .disposed(by: disposeBag)

                controller.establishConnection(trigger: trigger) ~~> scheduler

                let reconnectTrigger = PublishSubject<Void>()
                reconnectStrategy.stagedAction = { _ in .suspendUntil(reconnectTrigger.asObservable()) }

                connector.connectionStatus.onNext(.connecting)
                connector.connectionStatus.onError(TestError.someError)

                controller.disconnect(trigger: trigger)

                reconnectTrigger.onNext(()) ~~> scheduler
                connector.connectionStatus.onNext(.connecting)

                expect(events) == [.disconnected, .connecting, .disconnected]
            }
        }

        describe("reconnect strategy") {
            it("disconnects on unrecoverable error & handles new connection requests") {
                var events: [ConnectionStatus<Connection>] = []

                controller.connectionStatus
                    .subscribe(onNext: { events.append($0) })
                    .disposed(by: disposeBag)

                controller.establishConnection(trigger: trigger) ~~> scheduler

                connector.connectionStatus.onNext(.connecting)
                connector.connectionStatus.onError(TestError.someError)

                // Reset the old subject, otherwise it would re-emit the error
                connector.connectionStatus = PublishSubject()

                controller.establishConnection(trigger: trigger) ~~> scheduler

                connector.connectionStatus.onNext(.connecting)

                expect(events) == [.disconnected, .connecting, .disconnected, .connecting]
            }

            it("retries on recoverable error") {
                // Need a connector that prepares the connection status in advance
                final class ServedMockConnector: Connector {
                    var connectionStatuses: [Observable<ConnectionStatus<Connection>>] = []
                    private var nextServeIndex = 0
                    func establishConnection(descriptor: PeerDescriptor) -> Observable<ConnectionStatus<Connection>> {
                        defer { nextServeIndex += 1 }
                        return connectionStatuses[nextServeIndex]
                    }
                }

                let connector = ServedMockConnector()

                controller = ConnectionController(
                    connector: connector,
                    descriptor: descriptor,
                    reconnectStrategy: reconnectStrategy,
                    scheduler: scheduler,
                    debugIdentifier: "test"
                )

                var events: [ConnectionStatus<Connection>] = []

                controller.connectionStatus
                    .subscribe(onNext: { events.append($0) })
                    .disposed(by: disposeBag)

                reconnectStrategy.stagedAction = { _ in .retryWithDelay(.never) }

                connector.connectionStatuses = [
                    Observable.just(.connected(someConnection)).concat(Observable.error(TestError.someError)),
                    Observable.just(.connecting).concat(Observable.never())
                ]

                controller.establishConnection(trigger: trigger) ~~> scheduler

                expect(events) == [.disconnected, .connecting, .connected(someConnection), .disconnected, .connecting]
            }

            it("reconnects on trigger on recoverable error") {
                var events: [ConnectionStatus<Connection>] = []

                controller.connectionStatus
                    .subscribe(onNext: { events.append($0) })
                    .disposed(by: disposeBag)

                controller.establishConnection(trigger: trigger) ~~> scheduler

                let reconnectTrigger = PublishSubject<Void>()
                reconnectStrategy.stagedAction = { _ in .suspendUntil(reconnectTrigger) }

                connector.connectionStatus.onNext(.connected(someConnection))
                connector.connectionStatus.onError(TestError.someError)

                // Reset the old subject, otherwise it would re-emit the error
                connector.connectionStatus = PublishSubject()

                reconnectTrigger.onNext(()) ~~> scheduler
                connector.connectionStatus.onNext(.connecting)

                expect(events) == [.disconnected, .connecting, .connected(someConnection), .disconnected, .connecting]
            }

            it("resets the failure history on successful connection") {
                var resetHistory = false

                reconnectStrategy.resetFailureHistory
                    .subscribe(onNext: { resetHistory = true })
                    .disposed(by: disposeBag)

                controller.establishConnection(trigger: trigger) ~~> scheduler

                let reconnectTrigger = PublishSubject<Void>()
                reconnectStrategy.stagedAction = { _ in .suspendUntil(reconnectTrigger) }

                connector.connectionStatus.onNext(.connecting)
                connector.connectionStatus.onError(TestError.someError)

                // Reset the old subject, otherwise it would re-emit the error
                connector.connectionStatus = PublishSubject()

                reconnectTrigger.onNext(()) ~~> scheduler
                connector.connectionStatus.onNext(.connected(someConnection))

                expect(resetHistory) == true
            }
        }

        describe("metrics") {
            typealias MetricsEvent = ConnectionControllerMetricsEvent

            it("emits the correct metrics events when the connection status errors out") {
                var events: [MetricsEvent] = []

                controller.metrics.subscribe(onNext: { events.append($0) }).disposed(by: disposeBag)

                controller.establishConnection(trigger: trigger) ~~> scheduler

                connector.connectionStatus.onNext(.connecting)
                connector.connectionStatus.onNext(.connected(someConnection))

                connector.connectionStatus.onError(TestError.someError) ~~> scheduler

                var iterator = events.makeIterator()
                expect(events).to(haveCount(6))
                expect(iterator.next().map(stringify)) == stringify(MetricsEvent.connectionRequested(trigger: trigger))
                expect(iterator.next().map(stringify)) == stringify(MetricsEvent.stateChangedToConnecting)
                expect(iterator.next().map(stringify)) == stringify(MetricsEvent.stateChangedToConnected)
                expect(iterator.next().map(stringify)) == stringify(MetricsEvent.stateChangedToDisconnected)
                expect(iterator.next().map(stringify)) == stringify(MetricsEvent.encounteredError(TestError.someError))
                expect(iterator.next().map(stringify)) == stringify(MetricsEvent.determinedReconnectStrategyAction(.stop(TestError.someError)))
            }

            it("emits the correct metrics events when disconnection is requested") {
                var events: [MetricsEvent] = []

                controller.metrics.subscribe(onNext: { events.append($0) }).disposed(by: disposeBag)

                controller.establishConnection(trigger: trigger) ~~> scheduler

                connector.connectionStatus.onNext(.connecting)
                connector.connectionStatus.onNext(.connected(someConnection))

                controller.disconnect(trigger: trigger) ~~> scheduler

                var iterator = events.makeIterator()
                expect(events).to(haveCount(5))
                expect(iterator.next().map(stringify)) == stringify(MetricsEvent.connectionRequested(trigger: trigger))
                expect(iterator.next().map(stringify)) == stringify(MetricsEvent.stateChangedToConnecting)
                expect(iterator.next().map(stringify)) == stringify(MetricsEvent.stateChangedToConnected)
                expect(iterator.next().map(stringify)) == stringify(MetricsEvent.disconnectionRequested(trigger: trigger))
                expect(iterator.next().map(stringify)) == stringify(MetricsEvent.stateChangedToDisconnected)
            }
        }
    }
}

private enum TestError: Error {
    case someError
}

//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  ConnectionControllerSpec.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 12/6/17.
//

@testable import GoldenGate
import GoldenGateXP
import Nimble
import Quick
import RxCocoa
import RxSwift

// swiftlint:disable function_body_length
// swiftlint:disable force_try

private enum TestError: Error {
    case someError
}

class ConnectionControllerSpec: QuickSpec {
    private typealias Controller = ConnectionController<String, MockStack>

    override func spec() {
        var runLoop: GoldenGate.RunLoop!
        var disposeBag: DisposeBag!
        var connection: PublishSubject<Connection>!
        var connector: MockConnector!
        var controller: Controller!

        func setupController(_ closure: @escaping (AnyObserver<Connection>) -> Disposable) {
            runLoop = GoldenGate.RunLoop()
            runLoop.start()
            connector = MockConnector(connection: Observable.create(closure))
            controller = try! Controller(
                runLoop: runLoop,
                connector: connector,
                stackBuilder: MockStackBuilder(),
                descriptor: "foo"
            )
        }

        beforeEach {
            disposeBag = DisposeBag()
            setupController { _ in Disposables.create() }
        }

        afterEach {
            runLoop.terminate()
        }

        describe("forceDisconnect") {
            describe("disconnects") {
                it("disconnects pending connection intents") {
                    waitUntil { done in
                        controller.establishConnection()
                            .do(onDispose: done)
                            .subscribe()
                            .disposed(by: disposeBag)

                        controller.forceDisconnect()
                    }
                }

                it("active connects") {
                    setupController { observer in
                        observer.on(.next(MockConnection()))
                        return Disposables.create()
                    }

                    waitUntil { done in
                        controller.establishConnection()
                            .do(onDispose: done)
                            .subscribe()
                            .disposed(by: disposeBag)

                        controller.connection
                            .subscribe(onNext: { _ in
                                controller.forceDisconnect()
                            })
                            .disposed(by: disposeBag)
                    }
                }
            }
        }

        describe("clearDescriptor()") {
            it("changes the descriptor") {
                waitUntil { done in
                    controller.descriptor
                        .filter { $0 == nil }
                        .take(1)
                        .do(onDispose: done)
                        .subscribe()
                        .disposed(by: disposeBag)

                    controller.clearDescriptor()
                }
            }
        }

        describe("state") {
            it("changes depending on peer/link presence") {
                setupController { observer in
                    let networkLinkStatus = PublishSubject<NetworkLinkStatus>()
                    let peer = MockConnection(networkLinkStatus: networkLinkStatus)
                    observer.on(.next(peer))
                    networkLinkStatus.on(.next(.connecting))
                    networkLinkStatus.on(.next(.disconnected))
                    networkLinkStatus.on(.next(.connecting))
                    networkLinkStatus.on(.next(.negotiating(nil)))
                    networkLinkStatus.on(.next(.connected(MockLink())))
                    observer.on(.error(TestError.someError))
                    return Disposables.create()
                }

                waitUntil { done in
                    controller.state
                        .distinctUntilChanged()
                        .take(4)
                        .toArray()
                        .subscribe(onSuccess: { array in
                            expect(array).to(equal([
                                .disconnected,
                                .connecting,
                                // Even though the link becomes `.disconnected`,
                                // the existence of the peer indicates connection
                                // intent, so the value stays `.connecting`.
                                .connected,
                                .disconnected
                            ]))
                            done()
                        })
                        .disposed(by: disposeBag)

                    controller.establishConnection()
                        .catchError { _ in Completable.empty() }
                        .subscribe()
                        .disposed(by: disposeBag)
                }
            }
        }
    }
}

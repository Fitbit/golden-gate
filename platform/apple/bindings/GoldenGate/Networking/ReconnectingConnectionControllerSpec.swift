//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  ReconnectingConnectionControllerSpec.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 12/8/17.
//

import Foundation
@testable import GoldenGate
import GoldenGateXP
import Nimble
import Quick
import RxCocoa
import RxSwift
import RxTest

// swiftlint:disable function_body_length

private typealias Controller = ReconnectingConnectionController<MockConnectionController>

class MockReconnectStrategy: ReconnectStrategy {
    private let maxAttemptCount: Int
    private let delay: DispatchTimeInterval

    init(maxAttemptCount: Int, delay: DispatchTimeInterval) {
        self.maxAttemptCount = maxAttemptCount
        self.delay = delay
    }

    let resumeSubject = PublishSubject<Void>()

    var history = [Error]()

    func action(for error: Error) -> ReconnectStrategyAction {
        history.append(error)

        guard history.count < maxAttemptCount else {
            history.removeAll()
            return .suspendUntil(resumeSubject.asObservable())
        }

        return .retryWithDelay(delay)
    }
}

private enum TestError: Error {
    case testError
}

class ReconnectingConnectionControllerSpec: QuickSpec {
    override func spec() {
        let maxAttemptCount = 3
        var delay: DispatchTimeInterval = .seconds(0)
        var subscribeCount = 0
        var subscribed = false
        var disposeBag: DisposeBag!
        var reconnectStrategy: MockReconnectStrategy!
        var scheduler: HistoricalScheduler!
        var controller: Controller!

        func setupController(_ closure: @escaping (@escaping Completable.CompletableObserver, BehaviorRelay<ConnectionControllerState>) -> Disposable) {
            let mockController = MockConnectionController(
                sharedConnection: { state in
                    Completable.create(subscribe: { observer in
                        closure(observer, state)
                    })
                }
            )

            reconnectStrategy = MockReconnectStrategy(maxAttemptCount: maxAttemptCount, delay: delay)

            scheduler = HistoricalScheduler()

            controller = Controller(
                mockController,
                reconnectStrategy: reconnectStrategy,
                scheduler: scheduler
            )
        }

        beforeEach {
            disposeBag = DisposeBag()

            delay = .seconds(0)
            subscribeCount = 0
            subscribed = false

            setupController { _, state in
                subscribeCount += 1
                subscribed = true
                state.accept(.connecting)

                return Disposables.create {
                    subscribed = false
                }
            }
        }

        describe("retry") {
            it("happens \(maxAttemptCount - 1) times if the connection errors") {
                setupController { observer, _ in
                    subscribeCount += 1
                    expect(subscribeCount) <= maxAttemptCount

                    // erroring the connection should trigger a reconnect
                    observer(.error(TestError.testError))

                    return Disposables.create()
                }

                controller.establishConnection()
                    .subscribe()
                    .disposed(by: disposeBag)

                scheduler.advanceTo(scheduler.now.addingTimeInterval(0.100))
                expect(subscribeCount) == maxAttemptCount
            }

            it("can be delayed") {
                delay = .milliseconds(100)

                setupController { observer, _ in
                    subscribeCount += 1
                    expect(subscribeCount) <= maxAttemptCount

                    // erroring the connection should trigger a reconnect
                    observer(.error(TestError.testError))

                    return Disposables.create()
                }

                controller.establishConnection()
                    .subscribe()
                    .disposed(by: disposeBag)
                expect(subscribeCount) == 1

                scheduler.advanceTo(scheduler.now.addingTimeInterval(0.100))
                expect(subscribeCount) == 2

                scheduler.advanceTo(scheduler.now.addingTimeInterval(0.100))
                expect(subscribeCount) == 3
            }

            it("stops if the connection completes") {
                setupController { observer, _ in
                    subscribeCount += 1
                    observer(.completed)
                    return Disposables.create()
                }

                controller.establishConnection()
                    .subscribe()
                    .disposed(by: disposeBag)

                expect(subscribeCount) == 1
            }

            describe("when suspended") {
                beforeEach {
                    setupController { observer, _ in
                        subscribeCount += 1
                        observer(.error(TestError.testError))
                        return Disposables.create()
                    }

                    controller.establishConnection()
                        .subscribe()
                        .disposed(by: disposeBag)

                    scheduler.advanceTo(scheduler.now.addingTimeInterval(0.100))
                    expect(subscribeCount) == maxAttemptCount
                    subscribeCount = 0
                }

                it("resumes if the .suspendUntil(trigger) emits an element") {
                    reconnectStrategy.resumeSubject.onNext(())

                    scheduler.advanceTo(scheduler.now.addingTimeInterval(0.100))
                    expect(subscribeCount) == maxAttemptCount
                }

                it("resumes if establishConnection() is called") {
                    controller.establishConnection()
                        .subscribe()
                        .disposed(by: disposeBag)

                    scheduler.advanceTo(scheduler.now.addingTimeInterval(0.100))
                    expect(subscribeCount) == maxAttemptCount
                }
            }
        }

        it("disconnects if subscription is disposed") {
            var disconnected = false

            setupController { _, _ in
                return Disposables.create {
                    disconnected = true
                }
            }

            let subscription = controller.establishConnection().subscribe()
            subscription.dispose()

            expect(disconnected) == true
        }

        describe("clearDescriptor()") {
            it("disables reconnects") {
                var completed = false
                controller.establishConnection()
                    .do(onCompleted: { completed = true })
                    .subscribe()
                    .disposed(by: disposeBag)

                controller.clearDescriptor()
                expect(completed) == true
            }
        }

        describe("forceDisconnect()") {
            it("disables reconnects") {
                var completed = false
                controller.establishConnection()
                    .do(onCompleted: { completed = true })
                    .subscribe()
                    .disposed(by: disposeBag)

                controller.forceDisconnect()
                expect(completed) == true
            }
        }
    }
}

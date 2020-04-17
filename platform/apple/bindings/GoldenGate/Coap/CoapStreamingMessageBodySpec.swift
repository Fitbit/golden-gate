//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  CoapStreamingMessageBodySpec.swift
//  GoldenGate
//
//  Created by Vlad-Mihai Corneci on 18/01/2019.
//

import Nimble
import Quick
import RxBlocking
import RxSwift
import RxTest

@testable import GoldenGate

// swiftlint:disable force_try

class CoapStreamingMessageBodySpec: QuickSpec {
    override func spec() {
        var producerDispatchQueue: DispatchQueue!
        var coapStreamingMessage: CoapStreamingMessageBody!
        var sentData: Data!

        beforeEach {
            producerDispatchQueue = DispatchQueue(
                label: "CoapStreamingMessageBodySpec.producer",
                attributes: .concurrent
            )

            coapStreamingMessage = CoapStreamingMessageBody(transportReadiness: .just(.ready), requestIdentifier: "SOME_ID")
            sentData = Data()
        }

        it("serves the body correctly as a data stream") {
            let consumer = coapStreamingMessage
                    .asStream()
                    .toBlocking()

            producerDispatchQueue.async {
                for number in 0...1000 {
                    let encodedData = Data(number.encode())
                    coapStreamingMessage.onNext(encodedData)

                    sentData.append(encodedData)
                }
                coapStreamingMessage.onComplete()
            }

            let content = try! consumer.toArray()

            let receivedData = content.reduce(Data()) { (container, data) -> Data in
                var newContainer = container
                newContainer.append(data)

                return newContainer
            }

            expect(sentData).to(equal(receivedData))
        }

        it("serves the body correctly as a single block of data") {
            let consumer = coapStreamingMessage
                .asData()
                .toBlocking()

            producerDispatchQueue.async {
                for number in 0...1000 {
                    let encodedData = Data(number.encode())
                    coapStreamingMessage.onNext(encodedData)

                    sentData.append(encodedData)
                }
                coapStreamingMessage.onComplete()
            }

            let receivedData = try! consumer.single()

            expect(sentData).to(equal(receivedData))
        }

        describe("transport readiness") {
            var scheduler: TestScheduler!
            var response: TestableObservable<Data>!
            var transportReadiness: TestableObservable<TransportReadiness>!
            let transportUnavailableError = CoapRequestError.transportUnavailable(reason: TestError.transportNotReady)

            func makeMessageBody() -> CoapStreamingMessageBody {
                let body = CoapStreamingMessageBody(
                    transportReadiness: transportReadiness.asObservable(),
                    requestIdentifier: "SOME_ID"
                )

                _ = response.asObservable()
                    .subscribe(onNext: body.onNext, onCompleted: body.onComplete)

                return body
            }

            beforeEach {
                scheduler = TestScheduler(initialClock: 0)

                response = scheduler.createColdObservable([
                    .next(TestScheduler.Defaults.created + 15, sentData),
                    .completed(TestScheduler.Defaults.created + 20)
                    ]
                )
            }

            context("asData() tests") {
                it("fulfills asData() when transport is ready") {
                    transportReadiness = scheduler.createColdObservable([.next(0, .ready)])

                    let observer = scheduler.start {
                        makeMessageBody().asData().asObservable()
                    }

                    expect(observer.events).to(haveCount(2))
                    expect(observer.events[0]) == .next(TestScheduler.Defaults.subscribed + 20, sentData)
                    expect(observer.events[1]) == .completed(TestScheduler.Defaults.subscribed + 20)
                }

                it("rejects asData() when transport is not ready") {
                    transportReadiness = scheduler.createColdObservable([.next(0, .notReady(reason: TestError.transportNotReady))])

                    let observer = scheduler.start {
                        makeMessageBody().asData().asObservable()
                    }

                    expect(observer.events).to(haveCount(1))
                    expect(observer.events[0]) == .error(TestScheduler.Defaults.subscribed, transportUnavailableError)
                }

                it("rejects asData() when transport becomes not ready after asData() was subscribed") {
                    transportReadiness = scheduler.createColdObservable([
                        .next(0, .ready),
                        .next(10, .notReady(reason: TestError.transportNotReady))
                        ]
                    )

                    let observer = scheduler.start {
                        makeMessageBody().asData().asObservable()
                    }

                    expect(observer.events).to(haveCount(1))
                    expect(observer.events[0]) == .error(TestScheduler.Defaults.subscribed + 10, transportUnavailableError)
                }
            }

            context("asStream() tests") {
                it("fulfills asStream() when transport is ready") {
                    transportReadiness = scheduler.createColdObservable([.next(0, .ready)])

                    let observer = scheduler.start {
                        makeMessageBody().asStream()
                    }

                    expect(observer.events).to(haveCount(2))
                    expect(observer.events[0]) == .next(TestScheduler.Defaults.subscribed + 15, sentData)
                    expect(observer.events[1]) == .completed(TestScheduler.Defaults.subscribed + 20)
                }

                it("rejects asStream() when transport is not ready") {
                    transportReadiness = scheduler.createColdObservable([.next(0, .notReady(reason: TestError.transportNotReady))])

                    let observer = scheduler.start {
                        makeMessageBody().asStream()
                    }

                    expect(observer.events).to(haveCount(1))
                    expect(observer.events[0]) == .error(TestScheduler.Defaults.subscribed, transportUnavailableError)
                }

                it("rejects asStream() when transport becomes not ready after asStream() was subscribed") {
                    transportReadiness = scheduler.createColdObservable([
                        .next(0, .ready),
                        .next(10, .notReady(reason: TestError.transportNotReady))
                        ]
                    )

                    let observer = scheduler.start {
                        makeMessageBody().asStream()
                    }

                    expect(observer.events).to(haveCount(1))
                    expect(observer.events[0]) == .error(TestScheduler.Defaults.subscribed + 10, transportUnavailableError)
                }
            }
        }
    }
}

private extension CoapStreamingMessageBodySpec {
    enum TestError: Error {
        case transportNotReady
    }
}

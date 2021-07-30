//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  CoapEndpointSpec.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 6/6/18.
//

import BluetoothConnection
import Foundation
import GoldenGateXP
import Nimble
import Quick
import RxSwift

@testable import GoldenGate

// swiftlint:disable:next superfluous_disable_command
// swiftlint:disable function_body_length force_try

final class MockCoapMessageBody: CoapMessageBody {
    var response: Observable<Data> = .never()

    func asData() -> Single<Data> {
        return response.asSingle()
    }

    func asStream() -> Observable<Data> {
        return response
    }
}

final class CoapEndpointSpec: QuickSpec {
    override func spec() {
        var runLoop: GoldenGate.RunLoop!
        var transferStrategy: MockTransferStrategy!
        var transportReadiness: Observable<TransportReadiness>!

        func makeEndpoint() -> CoapEndpoint {
            return try! CoapEndpoint(
                runLoop: runLoop,
                port: .never(),
                transferStrategy: transferStrategy,
                transportReadiness: transportReadiness
            )
        }

        beforeEach {
            GoldenGate.RunLoop.assumeMainThreadIsRunLoop()
            runLoop = GoldenGate.RunLoop()
            runLoop.start()
            transferStrategy = MockTransferStrategy()
            transportReadiness = .never()
        }

        afterEach {
            runLoop.terminate()
        }

        describe("for non-2.xx responses") {
            beforeEach {
                transferStrategy.respondWith(responseCode: .serverError(.notImplemented))
            }

            it("emits a response if expectsSuccess is false") {
                let request = CoapRequestBuilder().expectsSuccess(false).build()

                waitUntil { done in
                    _ = makeEndpoint()
                        .response(request: request)
                        .subscribe(onSuccess: { message in
                            expect(message.code) == .response(.serverError(.notImplemented))
                            done()
                        })
                }
            }

            it("errors signal if expectsSuccess is true") {
                let request = CoapRequestBuilder().expectsSuccess(true).build()

                waitUntil { done in
                    _ = makeEndpoint()
                        .response(request: request)
                        .subscribe(onError: { error in
                            guard case CoapRequestError.responseNotSuccessful(let code, _) = error else {
                                fail()
                                return
                            }

                            expect(code) == .response(.serverError(.notImplemented))
                            done()
                        })
                }
            }
        }

        context("transport readiness") {
            let request = CoapRequestBuilder().build()
            let message = CoapMessage(code: .response(.success(.success)), options: [], body: MockCoapMessageBody())
            let transportUnavailableError = CoapRequestError.transportUnavailable(reason: TestError.transportNotReady)

            beforeEach {
                let response = Observable.just(message)
                    .delay(.milliseconds(20), scheduler: MainScheduler.instance).asSingle()
                transferStrategy.respondWith(response)
            }

            it("fulfills requests when transport is ready") {
                transportReadiness = .just(.ready)

                waitUntil { done in
                    _ = makeEndpoint()
                        .response(request: request)
                        .subscribe(onSuccess: { responseMessage in
                            expect(responseMessage.code) == message.code
                            done()
                        })
                }
            }

            it("rejects requests when transport is not ready") {
                transportReadiness = .just(.notReady(reason: TestError.transportNotReady))

                waitUntil { done in
                    _ = makeEndpoint()
                        .response(request: request)
                        .subscribe(onError: { error in
                            expect(error).to(matchError(transportUnavailableError))
                            done()
                        })
                }
            }

            it("rejects requests when transport becomes not ready after requests started") {
                transportReadiness = Observable.just(.ready)
                    .concat(
                        Observable.just(.notReady(reason: TestError.transportNotReady))
                            .delay(.milliseconds(10), scheduler: MainScheduler.instance).asSingle()
                    )

                waitUntil(timeout: .seconds(10)) { done in
                    _ = makeEndpoint()
                        .response(request: request)
                        .subscribe(onError: { error in
                            expect(error).to(matchError(transportUnavailableError))
                            done()
                        })
                }
            }
        }
    }
}

private extension CoapEndpointSpec {
    enum TestError: Error {
        case transportNotReady
    }
}

private class MockTransferStrategy: CoapTransferStrategy {
    enum Result {
        case none
        case message(CoapMessage)
        case response(Single<CoapMessage>)
        case error(Error)
    }

    private var result = Result.none

    func respondWith(error: Error) {
        self.result = .error(error)
    }

    func respondWith(message: CoapMessage) {
        self.result = .message(message)
    }

    func respondWith(responseCode: CoapCode.Response) {
        respondWith(message: CoapMessage(code: .response(responseCode), options: [], body: CoapStaticMessageBody(data: nil)))
    }

    func respondWith(_ response: Single<CoapMessage>) {
        self.result = .response(response)
    }

    func response(request: CoapRequest, endpoint: CoapEndpoint) -> Single<CoapMessage> {
        switch result {
        case .none:
            fatalError("Didn't expect request: \(request)")
        case .error(let error):
            return Single.error(error)
        case .response(let response):
            return response
        case .message(let message):
            return Single.just(message)
        }
    }
}

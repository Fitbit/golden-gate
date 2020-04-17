//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  RemoteShellSpec.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 2/28/18.
//

import FbSmo
import Foundation
@testable import GoldenGate
import GoldenGateXP
import Nimble
import Quick
import RxCocoa
import RxSwift

// swiftlint:disable:next superfluous_disable_command
// swiftlint:disable function_body_length force_try

private enum MockError: Error {
    case failZero
    case failOne
    case failTwo
    case failThree
}

class RemoteShellSpec: QuickSpec {
    override func spec() {
        var mockTransportLayer: MockTransportLayer!
        var disposeBag: DisposeBag!

        beforeEach {
            mockTransportLayer = MockTransportLayer()
            disposeBag = DisposeBag()
        }

        it("processes requests") {
            let shell = RemoteShell(transportLayer: mockTransportLayer)

            shell.register("ping", handler: { (params: TestParams) -> Single<String> in
                expect(params.secret).to(equal(666))
                return .just("pong")
            })

            shell.start()

            let request = TestRequest(
                id: 42,
                method: "ping",
                params: TestParams(secret: 666)
            )

            waitUntil { done in
                mockTransportLayer.sentData
                    .take(1)
                    .map(decode)
                    .subscribe(onNext: { (response: TestResponse) in
                        expect(response.id).to(equal(42))
                        expect(response.error).to(beNil())
                        expect(response.result).to(equal("pong"))
                        done()
                    })
                    .disposed(by: disposeBag)

                mockTransportLayer.receive(request)
            }
        }

        it("reports errors") {
            let shell = RemoteShell(transportLayer: mockTransportLayer)
            shell.register("fail") { _ in throw MockError.failThree }
            shell.start()

            let request = TestRequest(id: 42, method: "fail", params: nil)

            waitUntil { done in
                mockTransportLayer.sentData
                    .take(1)
                    .map(decode)
                    .subscribe(onNext: { (response: TestResponse) in
                        expect(response.id).to(equal(42))
                        expect(response.error).notTo(beNil())
                        expect(response.error?.code).to(equal(3))
                        expect(response.error?.data).notTo(beNil())

                        let message = response.error?.data?.message

                        // Will be the printed NSError description,
                        // which will be the generic "The operation couldn’t be completed"
                        // message but also include the error name + code.
                        expect(message).to(contain("MockError"))
                        expect(message).to(contain("The operation couldn’t be completed."))
                        done()
                    })
                    .disposed(by: disposeBag)

                mockTransportLayer.receive(request)
            }
        }
    }
}

private func decode<T: Decodable>(_ data: Data) throws -> T {
    let decoder = FbSmoDecoder()
    decoder.keyDecodingStrategy = .convertFromSnakeCase
    return try decoder.decode(T.self, from: data, using: .cbor)
}

private struct TestParams: Codable {
    let secret: Int
}

private struct TestRequest: Codable {
    let id: Int
    let method: String
    let params: TestParams?

    enum CodingKeys: CodingKey {
        case id
        case method
        case params
    }

    /// Needed to ensure an explicit `null` is encoded
    func encode(to encoder: Encoder) throws {
        var container = encoder.container(keyedBy: CodingKeys.self)

        try container.encode(id, forKey: .id)
        try container.encode(method, forKey: .method)

        if let params = params {
            try container.encode(params, forKey: .params)
        } else {
            try container.encodeNil(forKey: .params)
        }
    }
}

private struct TestResponse: Decodable {
    let id: Int
    let result: String?
    let error: TestError?
}

private struct TestError: Decodable {
    let code: Int
    let data: TestErrorData?
}

private struct TestErrorData: Decodable {
    let message: String?
}

private class MockTransportLayer: TransportLayer {
    var didReceiveData: Observable<Data> {
        return receivedData.asObservable()
    }

    let receivedData = PublishSubject<Data>()
    let sentData = PublishSubject<Data>()

    func connect() {
        // ignore
    }

    func send(data: Data) throws {
        sentData.on(.next(data))
    }

    private func send<T: Codable>(_ value: T) {
        let data = try! encode(value)
        sentData.on(.next(data))
    }

    func receive<T: Codable>(_ value: T) {
        let data = try! encode(value)
        receivedData.onNext(data)
    }

    private func encode<T: Encodable>(_ value: T) throws -> Data {
        let encoder = FbSmoEncoder()
        encoder.keyEncodingStrategy = .convertToSnakeCase
        return try encoder.encode(value, using: .cbor)
    }
}

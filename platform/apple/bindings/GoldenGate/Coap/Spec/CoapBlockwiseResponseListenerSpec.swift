//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  CoapBlockwiseResponseListenerSpec.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 5/30/18.
//

import Foundation
import GoldenGateXP
import Nimble
import Quick
import RxSwift

@testable import GoldenGate

// swiftlint:disable force_try function_body_length

class CoapBlockwiseResponseListenerSpec: QuickSpec {
    override func spec() {
        var listener: CoapBlockwiseResponseListener!
        var disposable: BooleanDisposable!

        beforeEach {
            listener = CoapBlockwiseResponseListener(transportReadiness: .just(.ready), requestIdentifier: "SOME_ID")
            disposable = BooleanDisposable()
        }

        it("accepts responses without payload") {
            let blockInfo = GG_CoapMessageBlockInfo(offset: 0, size: 16, more: false)
            listener.onResponse(blockInfo: blockInfo, payload: Data())

            // Release listener eagerly
            let response = listener.response(using: disposable)
            listener = nil

            waitUntil { done in
                _ = response
                    .flatMap { $0.body.asData() }
                    .subscribe(onSuccess: { data in
                        expect(disposable.isDisposed) == false
                        expect(data).to(beEmpty())
                        done()
                    })
            }
        }

        it("accepts single blocks") {
            let data = "1234567812345678".data(using: .utf8)!
            listener.onResponse(payload: data)

            // Release listener eagerly
            let response = listener.response(using: disposable)
            listener = nil

            waitUntil { done in
                _ = response
                    .flatMap { $0.body.asData() }
                    .subscribe(onSuccess: { receivedData in
                        expect(disposable.isDisposed) == false
                        expect(receivedData) == data
                        done()
                    })
            }
        }

        it("combines blocks") {
            let firstData = "1234567812345678".data(using: .utf8)!
            let firstBlockInfo = GG_CoapMessageBlockInfo(data: firstData, offset: 0, more: true)!
            listener.onResponse(blockInfo: firstBlockInfo, payload: firstData)

            let lastData = "ABCDEFGH".data(using: .utf8)!
            let lastBlockInfo = GG_CoapMessageBlockInfo(data: lastData, offset: 16, more: false)!
            listener.onResponse(blockInfo: lastBlockInfo, payload: lastData)

            // Release listener eagerly
            let response = listener.response(using: disposable)
            listener = nil

            waitUntil { done in
                _ = response
                    .flatMap { $0.body.asData() }
                    .subscribe(onSuccess: { data in
                        expect(disposable.isDisposed) == false
                        expect(data) == (firstData + lastData)
                        done()
                    })
            }
        }

        it("reports error on invalid response") {
            var messageRef: OpaquePointer?
            defer { GG_CoapMessage_Destroy(messageRef) }

            expect {
                try Data().withUnsafeBytes { (tokenBytes: UnsafeRawBufferPointer) -> Void in
                    try Data().withUnsafeBytes { (payloadBytes: UnsafeRawBufferPointer) in
                        try GG_CoapMessage_Create(
                            100,
                            GG_COAP_MESSAGE_TYPE_RST,
                            nil,
                            0,
                            0,
                            tokenBytes.baseAddress?.assumingMemoryBound(to: UInt8.self),
                            0,
                            payloadBytes.baseAddress?.assumingMemoryBound(to: UInt8.self),
                            0,
                            &messageRef
                        ).rethrow()
                    }
                }
            }.toNot(throwError())

            listener.onResponse(
                blockInfo: GG_CoapMessageBlockInfo(data: Data(), offset: 0, more: false)!,
                messageRef: messageRef!
            )

            // Release listener eagerly
            let response = listener.response(using: disposable)
            listener = nil

            waitUntil { done in
                _ = response
                    .subscribe(onError: { error in
                        expect(error).to(matchError(RawCoapMessageError.unexpectedCode(code: 0)))
                        expect(disposable.isDisposed) == false
                        done()
                    })
            }
        }

        it("reports errors on first block") {
            listener.onError(GGRawError.failure, message: "Custom Message")

            // Release listener eagerly
            let response = listener.response(using: disposable)
            listener = nil

            waitUntil { done in
                _ = response
                    .subscribe(onError: { error in
                        let expectedError = CoapBlockwiseResponseListenerError.failureWithMessage(GGRawError.failure, message: "Custom Message")
                        expect(error).to(matchError(expectedError))
                        expect(disposable.isDisposed) == false
                        done()
                    })
            }
        }

        it("reports body errors on later blocks") {
            let data = "1234567812345678".data(using: .utf8)!
            let blockInfo = GG_CoapMessageBlockInfo(data: data, offset: 0, more: true)!
            listener.onResponse(blockInfo: blockInfo, payload: data)
            listener.onError(GGRawError.failure, message: nil)

            // Release listener eagerly
            let response = listener.response(using: disposable)
            listener = nil

            waitUntil { done in
                _ = response
                    .subscribe(onSuccess: { response in
                        _ = response.body.asData().subscribe(onError: { error in
                            expect(error).to(matchError(GGRawError.failure))
                            done()
                        })
                    })
            }
        }

        it("reports extended error on first block") {
            let coapExtendedError = CoapExtendedError(code: 22, message: "22 is no good", namespace: "Howdy partner")
            // intentionally create a malformed block info with more set to true
            let blockInfo = GG_CoapMessageBlockInfo(offset: 0, size: 0, more: true)
            listener.onResponse(responseCode: .serverError(.internalServerError), blockInfo: blockInfo, payload: coapExtendedError.data!)

            // Release listener eagerly
            let response = listener.response(using: disposable)
            listener = nil

            waitUntil { done in
                _ = response
                    .flatMap { $0.extendedError }
                    .subscribe(onSuccess: { error in
                        expect(error).to(equal(coapExtendedError))
                        done()
                    })
            }
        }

        it("reports body errors on later blocks with failure code") {
            let data = "1234567812345678".data(using: .utf8)!
            let firstBlockInfo = GG_CoapMessageBlockInfo(data: data, offset: 0, more: true)!
            listener.onResponse(blockInfo: firstBlockInfo, payload: data)

            let coapExtendedError = CoapExtendedError(code: 22, message: "22 is no good", namespace: "Howdy  partner")
            let secondBlockInfo = GG_CoapMessageBlockInfo(offset: 0, size: 0, more: false)
            listener.onResponse(responseCode: .serverError(.internalServerError), blockInfo: secondBlockInfo, payload: coapExtendedError.data!)

            // Release listener eagerly
            let response = listener.response(using: disposable)
            listener = nil

            waitUntil { done in
                _ = response
                    .subscribe(onSuccess: { response in
                        _ = response.body.asData().subscribe(onError: { error in
                            guard case let CoapRequestError.responseNotSuccessful(code, extendedError) = error else {
                                fail("invalid error \(error)")
                                return
                            }
                            expect(code).to(equal(.response(.serverError(.internalServerError))))
                            expect(extendedError).to(equal(coapExtendedError))
                            done()
                        })
                    })
            }
        }

        it("assumes that order is guaranteed by XP") {
            // Note that `offset` parameters are completely bogus in this test.

            let firstData = "1234567812345678".data(using: .utf8)!
            let firstBlockInfo = GG_CoapMessageBlockInfo(data: firstData, offset: 16, more: true)!
            listener.onResponse(blockInfo: firstBlockInfo, payload: firstData)

            let lastData = "ABCDEFGH".data(using: .utf8)!
            let lastBlockInfo = GG_CoapMessageBlockInfo(data: lastData, offset: 8, more: false)!
            listener.onResponse(blockInfo: lastBlockInfo, payload: lastData)

            // Release listener eagerly
            let response = listener.response(using: disposable)
            listener = nil

            waitUntil { done in
                _ = response
                    .flatMap { $0.body.asData() }
                    .subscribe(onSuccess: { data in
                        expect(data) == (firstData + lastData)
                        done()
                    })
            }
        }
    }
}

/// Utility to synthesize responses
private extension CoapBlockwiseResponseListener {
    func onResponse(responseCode: CoapCode.Response = .success(.success), blockInfo: GG_CoapMessageBlockInfo? = nil, payload: Data) {
        let response = CoapResponseBuilder(messageId: 123, token: nil)
            .responseCode(responseCode)
            .body(data: payload)
            .build()

        let messageRef = try! response.asUnmanagedCoapMessage()
        defer { GG_CoapMessage_Destroy(messageRef) }

        var blockInfo = blockInfo ?? GG_CoapMessageBlockInfo(data: payload, offset: 0, more: false)!
        GG_CoapBlockwiseResponseListener_OnResponseBlock(
            ref,
            &blockInfo,
            messageRef
        )
    }

    func onError(_ error: GGRawError, message: String?) {
        GG_CoapBlockwiseResponseListener_OnError(
            ref,
            error.ggResult,
            (message != nil ? UnsafePointer<Int8>(strdup(message!)) : nil)
        )
    }
}

/// Utilities and shorthands to create GG_CoapMessageBlockInfo
private extension GG_CoapMessageBlockInfo {
    // [spec] The block size is encoded using a three-bit unsigned integer
    // (0 for 2**4 to 6 for 2**10 bytes), which we call the "SZX"
    // ("size exponent"); the actual block size is then "2**(SZX + 4)".
    //
    // NOTE: Using bit-shift instead of `pow`, so `+ 4` becomes `+ 3`.
    private static let blockSizes = (0...6).map { szx in 2 << (szx + 3) }

    init?(data: Data, offset: Int, more: Bool) {
        self.init(offset: offset, sizeGreaterOrEqualTo: data.count, more: more)
    }

    init?(offset: Int, sizeGreaterOrEqualTo arbitrarySize: Int, more: Bool) {
        guard
            let size = GG_CoapMessageBlockInfo.blockSizes
                .first(where: { $0 >= arbitrarySize })
        else {
            return nil
        }

        self.init(offset: offset, size: size, more: more)
    }
}

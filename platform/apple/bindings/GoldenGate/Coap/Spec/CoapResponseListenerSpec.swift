//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  CoapResponseListenerSpec.swift
//  GoldenGate
//
//  Created by Sylvain Rebaud on 10/12/18.
//

import Foundation
import Nimble
import Quick

@testable import GoldenGate
import GoldenGateXP

// swiftlint:disable:next superfluous_disable_command
// swiftlint:disable function_body_length force_try

class CoapResponseListenerSpec: QuickSpec {
    override func spec() {
        var listener: CoapResponseListener!
        var message: RawCoapMessage!

        beforeEach {
            listener = CoapResponseListener()
            message = RawCoapMessage(
                code: .response(.success(.success)),
                type: .confirmable,
                options: [],
                token: Data("token".utf8),
                payload: Data("payload".utf8),
                messageId: 0
            )
        }

        it("ignores ack") {
            listener.onAck()
        }

        it("handles valid response") {
            listener.onResponse(message)

            // Release listener eagerly
            let response = listener.response
            listener = nil

            waitUntil { done in
                _ = response
                    .flatMap { $0.body.asData() }
                    .subscribe(onSuccess: { data in
                        expect(data) == Data("payload".utf8)
                        done()
                    })
            }
        }

        it("handles invalid response") {
            let invalidCoapCode = CoapCode.CodeClass.request.rawValue | 0b000_11111
            listener.onResponse(message, rawCode: invalidCoapCode)

            // Release listener eagerly
            let response = listener.response
            listener = nil

            waitUntil { done in
                _ = response
                    .subscribe(onError: { error in
                        expect(error).to(matchError(RawCoapMessageError.unexpectedCode(code: 0)))
                        done()
                    })
            }
        }

        it("reports errors with a message") {
            listener.onError(GGRawError.failure, message: "Custom Message")

            // Release listener eagerly
            let response = listener.response
            listener = nil

            waitUntil { done in
                _ = response
                    .subscribe(onError: { error in
                        let expectedError = CoapResponseListenerError.failureWithMessage(GGRawError.failure, message: "Custom Message")
                        expect(error).to(matchError(expectedError))
                        done()
                    })
            }
        }

        it("reports errors without a message") {
            listener.onError(GGRawError.failure, message: nil)

            // Release listener eagerly
            let response = listener.response
            listener = nil

            waitUntil { done in
                _ = response
                    .subscribe(onError: { error in
                        expect(error).to(matchError(GGRawError.failure))
                        done()
                    })
            }
        }
    }
}

private extension CoapResponseListener {
    func onAck() {
        GG_CoapResponseListener_OnAck(ref)
    }

    func onError(_ error: GGRawError, message: String?) {
        GG_CoapResponseListener_OnError(
            ref,
            error.ggResult,
            (message != nil ? UnsafePointer<Int8>(strdup(message!)) : nil)
        )
    }

    func onResponse(_ message: RawCoapMessage, rawCode: UInt8? = nil) {
        let messageRef = try! message.asUnmanagedCoapMessage(rawCode: rawCode)
        defer { GG_CoapMessage_Destroy(messageRef) }

        GG_CoapResponseListener_OnResponse(ref, messageRef)
    }
}

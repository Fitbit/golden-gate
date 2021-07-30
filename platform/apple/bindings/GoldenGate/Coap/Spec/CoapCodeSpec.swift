//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  CoapCodeSpec.swift
//  GoldenGate
//
//  Created by Sylvain Rebaud on 10/12/18.
//

import Foundation
import GoldenGateXP
import Nimble
import Quick

@testable import GoldenGate

// swiftlint:disable:next superfluous_disable_command
// swiftlint:disable function_body_length force_try

class CoapCodeSpec: QuickSpec {
    override func spec() {
        var message: RawCoapMessage!

        beforeEach {
            message = RawCoapMessage(
                code: .request(.get),
                type: .confirmable,
                options: [],
                token: "token".data(using: .utf8)!,
                payload: .none,
                messageId: 0
            )
        }

        describe("CoapCode.Request") {
            it("can be restored") {
                expect {
                    _ = try message.withRequest(.get).asRestoredFromUnmanaged()
                    _ = try message.withRequest(.post).asRestoredFromUnmanaged()
                    _ = try message.withRequest(.put).asRestoredFromUnmanaged()
                    _ = try message.withRequest(.delete).asRestoredFromUnmanaged()
                }.toNot(throwError())
            }

            it("handles invalid code") {
                expect {
                    _ = try message.asRestoredFromUnmanaged {
                        try $0.asUnmanagedCoapMessage(rawCode: CoapCode.CodeClass.request.rawValue)
                    }
                }.to(throwError(RawCoapMessageError.unexpectedCode(code: 0)))
            }
        }

        describe("CoapCode.SuccessfulResponse") {
            it("can be restored") {
                expect {
                    _ = try message.withResponse(.success(.success)).asRestoredFromUnmanaged()
                    _ = try message.withResponse(.success(.created)).asRestoredFromUnmanaged()
                    _ = try message.withResponse(.success(.deleted)).asRestoredFromUnmanaged()
                    _ = try message.withResponse(.success(.valid)).asRestoredFromUnmanaged()
                    _ = try message.withResponse(.success(.changed)).asRestoredFromUnmanaged()
                    _ = try message.withResponse(.success(.content)).asRestoredFromUnmanaged()
                }.toNot(throwError())
            }
        }

        describe("CoapCode.ClientErrorResponse") {
            it("can be restored") {
                expect {
                    _ = try message.withResponse(.clientError(.badRequest)).asRestoredFromUnmanaged()
                    _ = try message.withResponse(.clientError(.unauthorized)).asRestoredFromUnmanaged()
                    _ = try message.withResponse(.clientError(.badOption)).asRestoredFromUnmanaged()
                    _ = try message.withResponse(.clientError(.forbidden)).asRestoredFromUnmanaged()
                    _ = try message.withResponse(.clientError(.notFound)).asRestoredFromUnmanaged()
                    _ = try message.withResponse(.clientError(.methodNotAllowed)).asRestoredFromUnmanaged()
                    _ = try message.withResponse(.clientError(.notAcceptable)).asRestoredFromUnmanaged()
                    _ = try message.withResponse(.clientError(.preconditionFailed)).asRestoredFromUnmanaged()
                    _ = try message.withResponse(.clientError(.requestEntityTooLarge)).asRestoredFromUnmanaged()
                    _ = try message.withResponse(.clientError(.unsupportedContentFormat)).asRestoredFromUnmanaged()
                }.toNot(throwError())
            }

            it("conforms to Equatable") {
                expect(CoapCode.Response.clientError(.badOption)) == CoapCode.Response.clientError(.badOption)
            }
        }

        describe("CoapCode.ServerErrorResponse") {
            it("can be restored") {
                expect {
                    _ = try message.withResponse(.serverError(.internalServerError)).asRestoredFromUnmanaged()
                    _ = try message.withResponse(.serverError(.notImplemented)).asRestoredFromUnmanaged()
                    _ = try message.withResponse(.serverError(.badGateway)).asRestoredFromUnmanaged()
                    _ = try message.withResponse(.serverError(.serviceUnavailable)).asRestoredFromUnmanaged()
                    _ = try message.withResponse(.serverError(.gatewayTimeout)).asRestoredFromUnmanaged()
                    _ = try message.withResponse(.serverError(.proxyingNotSupported)).asRestoredFromUnmanaged()
                }.toNot(throwError())
            }

            it("conforms to Equatable") {
                expect(CoapCode.Response.serverError(.internalServerError)) == CoapCode.Response.serverError(.internalServerError)

                expect(CoapCode.Response.serverError(.internalServerError)) != CoapCode.Response.clientError(.badOption)
                expect(CoapCode.Response.serverError(.internalServerError)) != CoapCode.Response.success(.success)
            }
        }
    }
}

extension RawCoapMessage {
    internal func asUnmanagedCoapMessage(rawCode: UInt8? = nil) throws -> OpaquePointer {
        let rawCode = rawCode ?? code.rawValue
        let options = CoapMessageOptionParam.from(self.options)
        let token = self.token ?? Data()
        let payload = self.payload ?? Data()

        var ref: OpaquePointer?
        try withExtendedLifetime(options) {
            try token.withUnsafeBytes { (tokenBytes: UnsafeRawBufferPointer) -> Void in
                try payload.withUnsafeBytes { (payloadBytes: UnsafeRawBufferPointer) in
                    try GG_CoapMessage_Create(
                        rawCode,
                        type.ref,
                        options?.ref,
                        options?.count ?? 0,
                        messageId,
                        tokenBytes.baseAddress?.assumingMemoryBound(to: UInt8.self),
                        token.count,
                        payloadBytes.baseAddress?.assumingMemoryBound(to: UInt8.self),
                        payload.count,
                        &ref
                        ).rethrow()
                }
            }
        }
        return ref!
    }
}

extension RawCoapMessage {
    typealias AsUnmanaged = (RawCoapMessage) throws -> OpaquePointer
    func asRestoredFromUnmanaged(_ asUnmanaged: AsUnmanaged = { try $0.asUnmanagedCoapMessage() }) throws -> RawCoapMessage {
        let requestRef = try asUnmanaged(self)
        defer { GG_CoapMessage_Destroy(requestRef) }
        let restoredMessage = try RawCoapMessage(message: requestRef)
        expect(restoredMessage).to(equal(self))
        return restoredMessage
    }
}

extension RawCoapMessage {
    func withRequest(_ request: CoapCode.Request) -> RawCoapMessage {
        return RawCoapMessage(
            code: .request(request),
            type: self.type,
            options: self.options,
            token: self.token,
            payload: self.payload,
            messageId: self.messageId
        )
    }

    func withResponse(_ response: CoapCode.Response) -> RawCoapMessage {
        return RawCoapMessage(
            code: .response(response),
            type: self.type,
            options: self.options,
            token: self.token,
            payload: self.payload,
            messageId: self.messageId
        )
    }
}

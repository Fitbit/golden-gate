//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  RawCoapMessageSpec.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 5/30/18.
//

// swiftlint:disable force_try

@testable import GoldenGate
import GoldenGateXP
import Nimble
import Quick

class RawCoapMessageSpec: QuickSpec {
    func testX() {
        // This is here because otherwise Quick tests don't appear in Test Navigator
    }

    override func spec() {
        it("parses messages") {
            let token = "hi".data(using: .utf8)!
            let data = "data".data(using: .utf8)!
            let response = CoapResponseBuilder(messageId: 123, token: token)
                .option(number: .uriHost, value: "example.org")
                .option(number: .ifMatch, value: "ANY")
                .option(number: .size1, value: .uint(4))
                .body(data: data)
                .build()

            let messageRef = try! response.asUnmanagedCoapMessage()
            defer { GG_CoapMessage_Destroy(messageRef) }

            let message = try! RawCoapMessage(message: messageRef)
            expect(message.code) == .response(.success(.success))
            expect(message.messageId) == 123
            expect(message.token) == token
            expect(message.options) == [
                // Even though we passed in a string, If-Match is always interpreted as `opaque`
                CoapOption(number: .ifMatch, value: .opaque("ANY".data(using: .utf8)!)),
                // Even though we passed this, If-Match (1) comes before URI-Host (3)
                CoapOption(number: .uriHost, value: .string("example.org")),
                CoapOption(number: .size1, value: .uint(4))
            ]

            expect(message.payload) == data
        }
    }
}

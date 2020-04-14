//
//  CoapResponseBuilderSpec.swift
//  GoldenGate-iOS
//
//  Created by Bogdan Vlad on 03/04/2018.
//  Copyright © 2018 Fitbit. All rights reserved.
//

import Nimble
import Quick

@testable import GoldenGate

class CoapResponseBuilderSpec: QuickSpec {
    override func spec() {
        var request: RawCoapMessage!

        beforeEach {
            request = RawCoapMessage(
                code: .request(.get),
                type: .confirmable,
                options: [],
                token: "token".data(using: .utf8)!,
                payload: .none,
                messageId: 0
            )
        }

        describe("init") {
            it("has the same token as the request") {
                let response = CoapResponseBuilder(request: request)
                    .build()

                expect(response.token).to(equal(request.token))
            }
        }

        describe("option()") {
            it("appends in order") {
                let request = CoapResponseBuilder(request: request)
                    .option(number: .ifMatch, value: "test".data(using: .utf8)!)
                    .option(number: .contentFormat, value: 5)
                    .option(number: .maxAge, value: "test")
                    .build()

                expect(request.options) == [
                    CoapOption(number: .ifMatch, value: .opaque("test".data(using: .utf8)!)),
                    CoapOption(number: .contentFormat, value: .uint(5)),
                    CoapOption(number: .maxAge, value: .string("test"))
                ]
            }
        }

        describe("responseCode()") {
            it("sets the response code") {
                let response = CoapResponseBuilder(request: request)
                    .responseCode(.success(.changed))
                    .build()

                expect(response.responseCode) == .success(.changed)
            }
        }

        it("sets the body") {
            let data = "hello".data(using: .utf8)!

            let response = CoapResponseBuilder(request: request)
                .body(data: data)
                .build()

            expect(response.body.data) == data
        }
    }
}

//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  CoapRequestBuilderSpec.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 5/29/18.
//

// swiftlint:disable function_body_length

@testable import GoldenGate
import Nimble
import Quick

class CoapRequestBuilderSpec: QuickSpec {
    func testX() {
        // This is here because otherwise Quick tests don't appear in Test Navigator
    }

    override func spec() {
        describe("by default") {
            var request: CoapRequest!

            beforeEach {
                request = CoapRequestBuilder().build()
            }

            it("has method set to GET") {
                expect(request.method) == CoapCode.Request.get
            }

            it("has path set to /") {
                expect(request.path) == "/"
            }

            it("has expectsSuccess set to `true`") {
                expect(request.expectsSuccess) == true
            }

            it("has confirmable set to `true`") {
                expect(request.confirmable) == true
            }

            it("has has no options") {
                expect(request.options).to(beEmpty())
            }
        }

        describe("method()") {
            it("sets the method") {
                let request = CoapRequestBuilder()
                    .method(.post)
                    .build()

                expect(request.method) == CoapCode.Request.post
            }

            it("replaces the method") {
                let request = CoapRequestBuilder()
                    .method(.post)
                    .method(.delete)
                    .build()

                expect(request.method) == CoapCode.Request.delete
            }
        }

        describe("confirmable()") {
            it("sets confirmable") {
                let request = CoapRequestBuilder()
                    .confirmable(false)
                    .build()

                expect(request.confirmable) == false
            }

            it("replaces confirmable") {
                let request = CoapRequestBuilder()
                    .confirmable(false)
                    .confirmable(true)
                    .build()

                expect(request.confirmable) == true
            }
        }

        describe("expectsSuccess()") {
            it("sets expectsSuccess") {
                let request = CoapRequestBuilder()
                    .expectsSuccess(false)
                    .build()

                expect(request.expectsSuccess) == false
            }

            it("replaces expectsSuccess") {
                let request = CoapRequestBuilder()
                    .expectsSuccess(false)
                    .expectsSuccess(true)
                    .build()

                expect(request.expectsSuccess) == true
            }
        }

        describe("path()") {
            it("sets the path") {
                let request = CoapRequestBuilder()
                    .path(["hello", "world"])
                    .build()

                expect(request.path) == "/hello/world"
                expect(request.options) == [
                    CoapOption(number: .uriPath, value: .string("hello")),
                    CoapOption(number: .uriPath, value: .string("world"))
                ]
            }

            it("replaces the path") {
                let request = CoapRequestBuilder()
                    .path(["hello", "world"])
                    .path(["hey", "universe"])
                    .build()

                expect(request.path) == "/hey/universe"
                expect(request.options) == [
                    CoapOption(number: .uriPath, value: .string("hey")),
                    CoapOption(number: .uriPath, value: .string("universe"))
                ]
            }

            it("prepends the path") {
                let request = CoapRequestBuilder()
                    .option(number: .contentFormat, value: 5)
                    .path(["hello", "world"])
                    .build()

                expect(request.options) == [
                    CoapOption(number: .uriPath, value: .string("hello")),
                    CoapOption(number: .uriPath, value: .string("world")),
                    CoapOption(number: .contentFormat, value: .uint(5))
                ]
            }
        }

        describe("option()") {
            it("appends in order") {
                let request = CoapRequestBuilder()
                    .option(number: .ifMatch, value: Data("test".utf8))
                    .option(number: .contentFormat, value: 5)
                    .option(number: .maxAge, value: "test")
                    .build()

                expect(request.options) == [
                    CoapOption(number: .ifMatch, value: .opaque(Data("test".utf8))),
                    CoapOption(number: .contentFormat, value: .uint(5)),
                    CoapOption(number: .maxAge, value: .string("test"))
                ]
            }
        }

        describe("acceptsBlockwiseTransfer()") {
            it("sets acceptsBlockwiseTransfer") {
                let request = CoapRequestBuilder()
                    .acceptsBlockwiseTransfer(false)
                    .build()

                expect(request.acceptsBlockwiseTransfer) == false
            }

            it("replaces acceptsBlockwiseTransfer") {
                let request = CoapRequestBuilder()
                    .acceptsBlockwiseTransfer(false)
                    .acceptsBlockwiseTransfer(true)
                    .build()

                expect(request.acceptsBlockwiseTransfer) == true
            }
        }

        describe("body()") {
            it("sets body") {
                let request = CoapRequestBuilder()
                    .body(data: Data("foo".utf8))
                    .build()

                expect(request.body) == CoapOutgoingBody.data(Data("foo".utf8), nil)
            }

            it("replaces acceptsBlockwiseTransfer") {
                let request = CoapRequestBuilder()
                    .body(data: Data("foo".utf8))
                    .body(data: Data("bar".utf8))
                    .build()

                expect(request.body) == CoapOutgoingBody.data(Data("bar".utf8), nil)
            }
        }

        it("has a description") {
            let request = CoapRequestBuilder()
                .method(.delete)
                .path(["betelgeuse", "V"])
                .build()

            expect(request.description) == "DELETE /betelgeuse/V"
        }
    }
}

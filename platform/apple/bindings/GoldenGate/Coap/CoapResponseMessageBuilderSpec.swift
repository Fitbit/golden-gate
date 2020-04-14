//
//  CoapResponseMessageBuilderSpec.swift
//  GoldenGate-iOS
//
//  Created by Bogdan Vlad on 03/04/2018.
//  Copyright © 2018 Fitbit. All rights reserved.
//

// swiftlint:disable force_try

@testable import GoldenGate
import Nimble
import Quick

class CoapResponseMessageBuilderSpec: QuickSpec {
    func testX() {
        // This is here because otherwise Quick tests don't appear in Test Navigator
    }
    
    // swiftlint:disable function_body_length
    override func spec() {
        var request: CoapMessage!

        beforeEach {
            request = CoapMessage(
                code: 0,
                type: .acknowledgement,
                options: [],
                token: "token".data(using: .utf8)!,
                tokenLength: 5,
                payload: nil,
                messageId: 0
            )
        }
        
        it("has the same token and token length as the request") {
            let responseBuilder = CoapResponseBuilder(request: request)
                .responseCode(CoapCode.successfulResponse(.changed))

            let response = try! responseBuilder.build()
            expect(response.token).to(equal(request.token))
        }

        it("sets the correct coap options") {
            let option1Properties = (CoapOption.Number.ifMatch, "test".data(using: .utf8)!)
            let option2Properties = (CoapOption.Number.contentFormat, 5 as UInt32)
            let option3Properties = (CoapOption.Number.maxAge, "test")

            let option1 = CoapOption(number: option1Properties.0, value: CoapOptionValue.opaque(option1Properties.1))
            let option2 = CoapOption(number: option2Properties.0, value: CoapOptionValue.uint(option2Properties.1))
            let option3 = CoapOption(number: option3Properties.0, value: CoapOptionValue.string(option3Properties.1))

            let responseBuilder = CoapResponseBuilder(request: request)
                .responseCode(CoapCode.successfulResponse(.changed))
                .option(number: option1.number, value: option1Properties.1)
                .option(number: option2.number, value: option2Properties.1)
                .option(number: option3.number, value: option3Properties.1)

            let response = try! responseBuilder.build()
            let option1Found = response.options.index { $0.value == CoapOptionValue.opaque(option1Properties.1) && $0.number.rawValue == option1.number.rawValue } != nil
            let option2Found = response.options.index { $0.value == CoapOptionValue.uint(option2Properties.1) && $0.number.rawValue == option2.number.rawValue } != nil
            let option3Found = response.options.index { $0.value == CoapOptionValue.string(option3Properties.1) && $0.number.rawValue == option3.number.rawValue } != nil

            expect(option1Found).to(beTrue())
            expect(option2Found).to(beTrue())
            expect(option3Found).to(beTrue())
        }

        it("sets the coap code") {
            let code = CoapCode.successfulResponse(.changed)
            let responseBuilder = CoapResponseBuilder(request: request)
                .responseCode(code)

            let response = try! responseBuilder.build()
            expect(response.code).to(equal(code.rawValue))
        }

        it("sets the body") {
            let data = "hello".data(using: .utf8)!
            let responseBuilder = CoapResponseBuilder(request: request)
                .responseCode(CoapCode.successfulResponse(.changed))
                .body(data: data)

            let response = try! responseBuilder.build()

            expect(response.payload).to(equal(data))
        }
    }
}

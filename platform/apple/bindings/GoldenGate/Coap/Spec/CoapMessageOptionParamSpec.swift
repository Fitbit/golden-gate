//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  CoapMessageOptionParamSpec.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 5/31/18.
//

import Foundation
@testable import GoldenGate
import Nimble
import Quick

class CoapMessageOptionParamSpec: QuickSpec {
    func testX() {
        // This is here because otherwise Quick tests don't appear in Test Navigator
    }

    override func spec() {
        var emptyOption: CoapOption!
        var stringOption: CoapOption!
        var uintOption: CoapOption!
        var opaqueOption: CoapOption!

        beforeEach {
            emptyOption = CoapOption(number: .accept, value: .empty)
            stringOption = CoapOption(number: .accept, value: .string("abc"))
            uintOption = CoapOption(number: .accept, value: .uint(123))
            opaqueOption = CoapOption(number: .accept, value: .opaque(Data("data".utf8)))
        }

        describe("count") {
            it("for a single element is 1") {
                let param = CoapMessageOptionParam(emptyOption)
                expect(param.count) == 1
            }

            it("is the number of params") {
                let lastParam = CoapMessageOptionParam(emptyOption)
                let firstParam = CoapMessageOptionParam(stringOption, next: lastParam)
                expect(firstParam.count) == 2
            }
        }

        describe("option") {
            it("returns the stored option") {
                expect(CoapMessageOptionParam(emptyOption).option) == emptyOption
                expect(CoapMessageOptionParam(stringOption).option) == stringOption
                expect(CoapMessageOptionParam(uintOption).option) == uintOption
                expect(CoapMessageOptionParam(opaqueOption).option) == opaqueOption
            }
        }

        describe("from") {
            it("returns nil for an empty list") {
                expect(CoapMessageOptionParam.from([])).to(beNil())
            }

            it("returns a linked list of options with the original order") {
                let param = CoapMessageOptionParam.from([emptyOption, stringOption, opaqueOption, uintOption])
                expect(Array(param!.makeIterator())) == [emptyOption, stringOption, opaqueOption, uintOption]
                expect(param!.count) == 4
            }
        }
    }
}

private extension CoapMessageOptionParam {
    func makeIterator() -> AnyIterator<CoapOption> {
        var current: CoapMessageOptionParam? = self

        return AnyIterator {
            let option = current?.option
            current = current?.next
            return option
        }
    }
}

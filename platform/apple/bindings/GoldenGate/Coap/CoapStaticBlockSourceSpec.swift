//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  CoapStaticBlockSourceSpec.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 6/1/18.
//

import Nimble
import Quick

@testable import GoldenGate
import GoldenGateXP

// swiftlint:disable:next superfluous_disable_command
// swiftlint:disable function_body_length force_try

class CoapStaticBlockSourceSpec: QuickSpec {
    override func spec() {
        var source: CoapStaticBlockSource!

        beforeEach {
            // 26 characters
            let alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ".data(using: .utf8)!
            source = CoapStaticBlockSource(data: alphabet)
        }

        describe("getDataSize") {
            it("returns (size, true) for requests inside data") {
                let (size, more) = try! source.getDataSize(offset: 0, size: 16)
                expect(size) == 16
                expect(more) == true
            }

            it("throws for requests 'behind' data") {
                expect {
                    _ = try source.getDataSize(offset: 32, size: 16)
                }.to(throwError())
            }

            it("returns (remaining, false) for requests ending beyond data") {
                let (size, more) = try! source.getDataSize(offset: 0, size: 64)
                expect(size) == 26
                expect(more) == false
            }
        }

        describe("getData") {
            it("writes the data to the destination") {
                var data = Data(count: 4)

                data.withUnsafeMutableBytes { (destination: UnsafeMutableRawBufferPointer) in
                    try! source.getData(
                        offset: 0,
                        size: 4,
                        destination: destination.baseAddress!.assumingMemoryBound(to: UInt8.self)
                    )
                }

                expect(data) == "ABCD".data(using: .utf8)
            }

            // Disabled test because `throwAssertion()` doesn't work reliably anymore.
            // See: https://github.com/Quick/Nimble/issues/478
            //
            // swiftlint:disable:next disabled_quick_test
            xit("asserts against OOB (expects XP to guard against this)") {
                var data = Data(count: 16)

                expect {
                    data.withUnsafeMutableBytes { (destination: UnsafeMutableRawBufferPointer) in
                        try! source.getData(
                            offset: 64,
                            size: 16,
                            destination: destination.baseAddress!.assumingMemoryBound(to: UInt8.self)
                        )
                    }
                }.to(throwAssertion())
            }
        }
    }
}

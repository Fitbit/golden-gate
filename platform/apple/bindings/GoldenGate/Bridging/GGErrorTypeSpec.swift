//
//  GGErrorTypeSpec.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 12/6/17.
//  Copyright © 2017 Fitbit. All rights reserved.
//

@testable import GoldenGate
import GoldenGateXP
import Nimble
import Quick

private enum TestError: Error {
    case someError
}

class GGErrorTypeSpec: QuickSpec {
    override func spec() {
        describe("GG_RawError") {
            it("should conform to CustomNSError") {
                let error = GGRawError(GG_ERROR_INTERNAL)
                expect(GGRawError.errorDomain) == "com.fitbit.goldengate"
                expect(error.errorCode) == Int(GG_ERROR_INTERNAL)
                expect(error.errorUserInfo).to(beEmpty())
            }
        }
        
        describe("GG_Result") {
            describe("rethrow()") {
                it("do nothing if result is GG_SUCCESS") {
                    let result = GG_SUCCESS

                    expect {
                        try result.rethrow()
                    }.toNot(throwError())
                }

                it("throws GGRawError if result not GG_SUCCESS") {
                    let result: GG_Result = GG_ERROR_INTERNAL

                    expect {
                        try result.rethrow()
                    }.to(throwError())
                }
            }

            describe("from()") {
                it("converts GGRawError objects to the respective GG_Result") {
                    let result = GG_Result.from { throw GGRawError(GG_ERROR_TIMEOUT) }
                    expect(result).to(equal(GG_ERROR_TIMEOUT))
                }

                it("converts arbitrary errors to GG_FAILURE") {
                    let result = GG_Result.from { throw TestError.someError }
                    expect(result).to(equal(GG_FAILURE))
                }
            }

            it("should conform to Equatable") {
                expect(GGRawError.timeout) == GGRawError.timeout
            }
        }
    }
}

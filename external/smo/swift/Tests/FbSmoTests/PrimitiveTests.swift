// Copyright 2016-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

import XCTest
@testable import FbSmo

class PrimitiveTests: XCTestCase {
    func testNil() {
        encodeDecodeTest([nil] as [String?])
        encodeDecodeTest(["x": nil] as [String: String?])
    }

    func testUndefined() {
        encodeDecodeTest(.symbol(.undefined))
    }

    func testBoolean() {
        encodeDecodeTest(true)
        encodeDecodeTest(false)
    }

    func testInteger8() {
        encodeDecodeTest(Int8.min)
        encodeDecodeTest(-1 as Int8)
        encodeDecodeTest(0 as Int8)
        encodeDecodeTest(Int8.max)
        encodeDecodeTest(UInt8.min)
        encodeDecodeTest(UInt8.max)
    }

    func testInteger16() {
        encodeDecodeTest(Int16.min)
        encodeDecodeTest(-1 as Int16)
        encodeDecodeTest(0 as Int16)
        encodeDecodeTest(Int16.max)
        encodeDecodeTest(UInt16.min)
        encodeDecodeTest(UInt16.max)
    }

    func testInteger32() {
        encodeDecodeTest(Int32.min)
        encodeDecodeTest(-1 as Int32)
        encodeDecodeTest(0 as Int32)
        encodeDecodeTest(Int32.max)
        encodeDecodeTest(UInt32.min)
        encodeDecodeTest(UInt32.max)
    }

    func testInteger64() {
        encodeDecodeTest(Int64.min)
        encodeDecodeTest(-1 as Int64)
        encodeDecodeTest(0 as Int64)
        encodeDecodeTest(Int64.max)
        encodeDecodeTest(UInt64.min)

        XCTAssertThrowsError(try encodeDecode(UInt64.max)) { error in
            XCTAssertEqual(error.rawDebugDescription, "Unable to encode \(UInt64.max) to SMO.")
        }
    }

    func testPlatformInteger() {
        encodeDecodeTest(Int.min)
        encodeDecodeTest(-1 as Int)
        encodeDecodeTest(0 as Int)
        encodeDecodeTest(Int.max)
        encodeDecodeTest(UInt.min)

        if UInt.bitWidth <= 32 {
            encodeDecodeTest(UInt.max)
        } else {
            XCTAssertThrowsError(try encodeDecode(UInt.max)) { error in
                XCTAssertEqual(error.rawDebugDescription, "Unable to encode \(UInt.max) to SMO.")
            }
        }
    }

    func testString() {
        encodeDecodeTest("")
        encodeDecodeTest("hello!")
    }

    func testData() {
        encodeDecodeTest(Data())
        encodeDecodeTest("hello!".data(using: .utf8)!)
    }

    func testFloat() throws {
        let values: [Float] = [0, .leastNonzeroMagnitude, .leastNormalMagnitude, .pi, .greatestFiniteMagnitude, .infinity]

        for value in values {
            encodeDecodeTest(value)
            encodeDecodeTest(-value)
        }

        XCTAssert((try encodeDecode(Float.nan)).isNaN)
        XCTAssert((try encodeDecode(Float.nan, using: .cbor)).isNaN)
    }

    func testDouble() throws {
        let values: [Double] = [0, .leastNonzeroMagnitude, .leastNormalMagnitude, .pi, .greatestFiniteMagnitude, .infinity]

        for value in values {
            encodeDecodeTest(value)
            encodeDecodeTest(-value)
        }

        XCTAssert((try encodeDecode(Double.nan)).isNaN)
        XCTAssert((try encodeDecode(Double.nan, using: .cbor)).isNaN)
    }

    func testArray() {
        encodeDecodeTest([1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16] as [Int])
    }
}

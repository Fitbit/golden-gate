// Copyright 2016-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

import XCTest
@testable import FbSmo

class ConversionTests: XCTestCase {
    func testToFloat() throws {
        XCTAssertEqual(try FbSmoDecoder().decode(Float.self, from: .integer(0)), 0)
        XCTAssertEqual(try FbSmoDecoder().decode(Float.self, from: .float(0)), 0)
        XCTAssertEqual(try FbSmoDecoder().decode(Float.self, from: .float(.infinity)), .infinity)
        XCTAssert((try FbSmoDecoder().decode(Float.self, from: .float(.nan))).isNaN)

        XCTAssertThrowsError(try FbSmoDecoder().decode(Float.self, from: .float(.leastNonzeroMagnitude))) { error in
            XCTAssertEqual(error.rawDebugDescription, "Expected to decode Float but found a double-precision, floating-point value instead.")
        }

        XCTAssertThrowsError(try FbSmoDecoder().decode(Float.self, from: .integer(.max))) { error in
            XCTAssertEqual(error.rawDebugDescription, "Expected to decode Float but found an integer instead.")
        }
    }

    func testToDouble() throws {
        XCTAssertEqual(try FbSmoDecoder().decode(Double.self, from: .integer(0)), 0)
        XCTAssertEqual(try FbSmoDecoder().decode(Double.self, from: .float(0)), 0)
        XCTAssertEqual(try FbSmoDecoder().decode(Double.self, from: .float(.infinity)), .infinity)
        XCTAssert((try FbSmoDecoder().decode(Double.self, from: .float(.nan))).isNaN)

        XCTAssertThrowsError(try FbSmoDecoder().decode(Double.self, from: .integer(.max))) { error in
            XCTAssertEqual(error.rawDebugDescription, "Expected to decode Double but found an integer instead.")
        }
    }
}

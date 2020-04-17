// Copyright 2016-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

import XCTest
@testable import FbSmo

class FbSmoCodingKeyTests: XCTestCase {
    func testEquals() {
        XCTAssertEqual(FbSmoCodingKey(intValue: 0), FbSmoCodingKey(intValue: 0))
        XCTAssertNotEqual(FbSmoCodingKey(intValue: 1), FbSmoCodingKey(intValue: 0))

        XCTAssertEqual(FbSmoCodingKey(stringValue: "foo"), FbSmoCodingKey(stringValue: "foo"))
        XCTAssertNotEqual(FbSmoCodingKey(stringValue: "bar"), FbSmoCodingKey(stringValue: "foo"))
    }

    func testHashValue() {
        XCTAssertEqual(FbSmoCodingKey(intValue: 0).hashValue, FbSmoCodingKey(intValue: 0).hashValue)
        XCTAssertNotEqual(FbSmoCodingKey(intValue: 1).hashValue, FbSmoCodingKey(intValue: 0).hashValue)

        XCTAssertEqual(FbSmoCodingKey(stringValue: "foo").hashValue, FbSmoCodingKey(stringValue: "foo").hashValue)
        XCTAssertNotEqual(FbSmoCodingKey(stringValue: "bar").hashValue, FbSmoCodingKey(stringValue: "foo").hashValue)
    }

    func testCopyIntValue() {
        let value = FbSmoCodingKey(intValue: 0)
        XCTAssertEqual(FbSmoCodingKey(value), value)
    }

    func testCopyStringValue() {
        let value = FbSmoCodingKey(stringValue: "foo")
        XCTAssertEqual(FbSmoCodingKey(value), value)
    }
}

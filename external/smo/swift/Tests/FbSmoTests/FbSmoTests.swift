// Copyright 2016-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

import XCTest
@testable import FbSmo

class FbSmoTests: XCTestCase {
    func testExpressible() {
        func test(_ expression1: FbSmo, _ expression2: FbSmo) {
            XCTAssertEqual(expression1, expression2)
        }

        test(nil, .symbol(.null))
        test(true, .symbol(true))
        test(false, .symbol(false))
        test("foo", .string("foo"))
        test(42, .integer(42))
        test(3.14, .float(Double(3.14 as Float)))
        test([42], .array([.integer(42)]))
        test(["hello": "world"], .object(["hello": .string("world")]))
    }

    func testEquals() {
        func assertEqual(_ expression1: FbSmo, _ expression2: FbSmo) {
            XCTAssertEqual(expression1, expression2)
        }

        assertEqual(nil, nil)
        assertEqual(.symbol(.undefined), .symbol(.undefined))
        assertEqual(true, true)
        assertEqual(false, false)
        assertEqual("foo", "foo")
        assertEqual(42, 42)
        assertEqual(3.14, 3.14)
        assertEqual([42], [42])
        assertEqual(["hello": "world"], ["hello": "world"])
        assertEqual(.bytes(Data()), .bytes(Data()))
    }

    func testNotEquals() {
        func assertNotEqual(_ expression1: FbSmo, _ expression2: FbSmo) {
            XCTAssertNotEqual(expression1, expression2)
        }

        assertNotEqual(nil, 1)
        assertNotEqual(.symbol(.undefined), nil)
        assertNotEqual(true, nil)
        assertNotEqual(false, nil)
        assertNotEqual("foo", nil)
        assertNotEqual(42, nil)
        assertNotEqual(3.14, nil)
        assertNotEqual([42], nil)
        assertNotEqual(["hello": "world"], nil)
        assertNotEqual(.bytes(Data()), nil)
    }
}

//
//  CBORTests.swift
//  SwiftCBORTests
//
//  Created by Maximilian Alexander on 4/22/19.
//

import XCTest
@testable import SwiftCBOR

class CBORTests: XCTestCase {
    static var allTests = [
        ("testSubscriptSetter", testSubscriptSetter),
        ("testNestedSubscriptSetter", testNestedSubscriptSetter),
        ("testNestedSubscriptSetterWithNewMap", testNestedSubscriptSetterWithNewMap)
    ]

    func testSubscriptSetter() {
        let dictionary: [String: Any] = [
            "foo": 1,
            "bar": "a",
            "zwii": "hd",
            "tags": [
                "a": "1",
                "b": 2
            ]
        ]

        let cborEncoded: [UInt8] = try! CBOR.encodeMap(dictionary)
        var cbor = try! CBOR.decode(cborEncoded)!
        cbor["foo"] = "changed"
        XCTAssertEqual(cbor["foo"], "changed")
    }

    func testNestedSubscriptSetter() {
        let dictionary: [String: Any] = [
            "foo": 1,
            "bar": "a",
            "zwii": "hd",
            "tags": [
                "a": "1",
                "b": 2
            ]
        ]

        let cborEncoded: [UInt8] = try! CBOR.encodeMap(dictionary)
        var cbor = try! CBOR.decode(cborEncoded)!
        cbor["tags"]?[2] = "changed"
        XCTAssertEqual(cbor["tags"]?[2], "changed")
    }

    func testNestedSubscriptSetterWithNewMap() {
        let dictionary: [String: Any] = [
            "foo": 1,
            "bar": "a",
            "zwii": "hd",
            "tags": [
                "a": "1",
                "b": 2
            ]
        ]

        let cborEncoded: [UInt8] = try! CBOR.encodeMap(dictionary)
        var cbor = try! CBOR.decode(cborEncoded)!

        let nestedMap: [CBOR: CBOR] = [
            "joe": "schmoe",
            "age": 56
        ]

        cbor["tags"]?[2] = CBOR.map(nestedMap)
        XCTAssertEqual(cbor["tags"]?[2], CBOR.map(nestedMap))
    }
}

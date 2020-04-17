// Copyright 2016-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

import XCTest
@testable import FbSmo

class CustomDecodingTests: XCTestCase {
    func testDecodeUnkeyed() throws {
        let smo: FbSmo = [nil, "1", nil, [1, 2], ["x": 11, "y": 22]]
        let decoded = try FbSmoDecoder().decode(SomeUnkeyedCodingObject.self, from: smo)
        XCTAssertEqual(decoded.isSuperNil, true)
        XCTAssertEqual(decoded.first, "1")
        XCTAssertEqual(decoded.second, nil)
        XCTAssertEqual(decoded.unkeyedTuple.0, 1)
        XCTAssertEqual(decoded.unkeyedTuple.1, 2)
        XCTAssertEqual(decoded.keyedTuple.x, 11)
        XCTAssertEqual(decoded.keyedTuple.y, 22)
        XCTAssertEqual(decoded.last, nil)
    }

    func testDecodeKeyed() throws {
        let smo: FbSmo = ["super": nil, "otherSuper": nil, "first": "1", "unkeyedTuple": [1, 2], "keyedTuple": ["x": 11, "y": 22]]
        let decoded = try FbSmoDecoder().decode(SomeKeyedCodingObject.self, from: smo)
        XCTAssertEqual(decoded.isSuperNil, true)
        XCTAssertEqual(decoded.first, "1")
        XCTAssertEqual(decoded.second, nil)
        XCTAssertEqual(decoded.unkeyedTuple.0, 1)
        XCTAssertEqual(decoded.unkeyedTuple.1, 2)
        XCTAssertEqual(decoded.keyedTuple.x, 11)
        XCTAssertEqual(decoded.keyedTuple.y, 22)
    }
}

enum KeyedTupleCodingKeys: String, CodingKey {
    case x
    case y
}

class SomeUnkeyedCodingObject: Decodable {
    let isSuperNil: Bool

    let first: String?
    let second: String?

    let unkeyedTuple: (Int, Int)
    let keyedTuple: (x: Int, y: Int)

    let last: String?

    required init(from decoder: Decoder) throws {
        var container = try decoder.unkeyedContainer()
        XCTAssertEqual(container.count, 5)

        isSuperNil = try container.superDecoder().singleValueContainer().decodeNil()

        first = try container.decodeIfPresent(String.self)
        second = try container.decodeIfPresent(String.self)

        var unkeyedTupleContainer = try container.nestedUnkeyedContainer()
        unkeyedTuple = (
            try unkeyedTupleContainer.decode(Int.self),
            try unkeyedTupleContainer.decode(Int.self)
        )

        let keyedTupleContainer = try container.nestedContainer(keyedBy: KeyedTupleCodingKeys.self)
        keyedTuple = (
            x: try keyedTupleContainer.decode(Int.self, forKey: .x),
            y: try keyedTupleContainer.decode(Int.self, forKey: .y)
        )

        last = try container.decodeIfPresent(String.self)
    }
}

class SomeKeyedCodingObject: Decodable {
    let isSuperNil: Bool
    let isCustomSuperNil: Bool

    let first: String?
    let second: String?

    let unkeyedTuple: (Int, Int)
    let keyedTuple: (x: Int, y: Int)

    enum MyCodingKeys: String, CodingKey {
        case customSuper
        case first
        case second
        case unkeyedTuple
        case keyedTuple
    }

    required init(from decoder: Decoder) throws {
        let container = try decoder.container(keyedBy: MyCodingKeys.self)
        XCTAssertFalse(container.allKeys.isEmpty)

        isSuperNil = try container.superDecoder().singleValueContainer().decodeNil()
        isCustomSuperNil = try container.superDecoder(forKey: .customSuper).singleValueContainer().decodeNil()

        first = try container.decodeIfPresent(String.self, forKey: .first)
        second = try container.decodeIfPresent(String.self, forKey: .second)

        var unkeyedTupleContainer = try container.nestedUnkeyedContainer(forKey: .unkeyedTuple)
        unkeyedTuple = (
            try unkeyedTupleContainer.decode(Int.self),
            try unkeyedTupleContainer.decode(Int.self)
        )

        let keyedTupleContainer = try container.nestedContainer(keyedBy: KeyedTupleCodingKeys.self, forKey: .keyedTuple)
        keyedTuple = (
            x: try keyedTupleContainer.decode(Int.self, forKey: .x),
            y: try keyedTupleContainer.decode(Int.self, forKey: .y)
        )
    }
}

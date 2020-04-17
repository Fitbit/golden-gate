// Copyright 2016-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

import XCTest
@testable import FbSmo

class NestedArrays2Tests: XCTestCase {
    private let expected: ArrayOrNumber = [[[[[1], 2], 3], 4], 5]

    /// Decode using JSONDecoder as a "baseline" check of our
    /// custom Codable implementation logic.
    func testJson() throws {
        let fixture = loadFixture("nested_arrays2.json")
        let decoded = try JSONDecoder().decode(type(of: expected), from: fixture)
        XCTAssertEqual(decoded, expected)
    }

    func testCbor() throws {
        let fixture = loadFixture("nested_arrays2.cbor")
        let decoded = try FbSmoDecoder().decode(type(of: expected), from: fixture, using: .cbor)
        XCTAssertEqual(decoded, expected)
    }

    func testEncodeDecode() {
        encodeDecodeTest(expected)
    }
}

private enum ArrayOrNumber: Codable, Equatable, ExpressibleByIntegerLiteral, ExpressibleByArrayLiteral, CustomStringConvertible {
    case array([ArrayOrNumber])
    case number(Int)

    public init(integerLiteral value: Int) {
        self = .number(value)
    }

    public init(arrayLiteral elements: ArrayOrNumber...) {
        self = .array(elements)
    }

    init(from decoder: Decoder) throws {
        let container = try decoder.singleValueContainer()

        // Try parsing it as a number first
        guard let number = try? container.decode(Int.self) else {
            self = .array(try container.decode([ArrayOrNumber].self))
            return
        }

        self = .number(number)
    }

    func encode(to encoder: Encoder) throws {
        switch self {
        case .array(let array):
            try array.encode(to: encoder)
        case .number(let number):
            var container = encoder.singleValueContainer()
            try container.encode(number)
        }
    }

    public var description: String {
        switch self {
        case .array(let array):
            let joined = array.map { $0.description }.joined(separator: ", ")
            return "[\(joined)]"
        case .number(let number):
            return number.description
        }
    }

    static func == (lhs: ArrayOrNumber, rhs: ArrayOrNumber) -> Bool {
        switch (lhs, rhs) {
        case let (.number(lhsNumber), .number(rhsNumber)):
            return lhsNumber == rhsNumber
        case (.number, _):
            return false
        case let (.array(lhsArray), .array(rhsArray)):
            return lhsArray == rhsArray
        case (.array, _):
            return false
        }
    }
}

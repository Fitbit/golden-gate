// Copyright 2016-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

import XCTest
@testable import FbSmo

class SuperTests: XCTestCase {
    private let expected = Child(parent: "parent", child: "child")

    /// Decode using JSONDecoder as a "baseline" check of our
    /// custom Codable implementation logic.
    func testJson() throws {
        let encoded = try JSONEncoder().encode(expected)
        let processed = try JSONDecoder().decode(type(of: expected), from: encoded)
        XCTAssertEqual(processed, expected)
    }

    func testCbor() throws {
        let encoded = try FbSmoEncoder().encode(expected)
        let processed = try FbSmoDecoder().decode(type(of: expected), from: encoded)
        XCTAssertEqual(processed, expected)
    }
}

private class Parent: Codable, Equatable {
    let parent: String

    private enum CodingKeys: String, CodingKey {
        case parent
    }

    init(parent: String) {
        self.parent = parent
    }

    required init(from decoder: Decoder) throws {
        let container = try decoder.container(keyedBy: CodingKeys.self)
        self.parent = try container.decode(String.self, forKey: CodingKeys.parent)
    }

    func encode(to encoder: Encoder) throws {
        var container = encoder.container(keyedBy: CodingKeys.self)
        try container.encode(parent, forKey: CodingKeys.parent)
    }

    static func == (lhs: Parent, rhs: Parent) -> Bool {
        return lhs.parent == rhs.parent
    }
}

private class Child: Parent {
    let child: String

    private enum CodingKeys: String, CodingKey {
        case child
    }

    init(parent: String, child: String) {
        self.child = child
        super.init(parent: parent)
    }

    required init(from decoder: Decoder) throws {
        let container = try decoder.container(keyedBy: CodingKeys.self)
        self.child = try container.decode(String.self, forKey: CodingKeys.child)
        try super.init(from: try container.superDecoder())
    }

    override func encode(to encoder: Encoder) throws {
        var container = encoder.container(keyedBy: CodingKeys.self)
        try container.encode(child, forKey: CodingKeys.child)
        try super.encode(to: container.superEncoder())
    }
}

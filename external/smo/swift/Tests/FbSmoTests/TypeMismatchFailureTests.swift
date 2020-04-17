// Copyright 2016-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

import XCTest
@testable import FbSmo

class TypeMismatchTests: XCTestCase {
    func testTypeMismatch() {
        let smo: FbSmo = [
            "name": true,
        ]

        XCTAssertThrowsError(try FbSmoDecoder().decode(Child.self, from: smo)) { error in
            XCTAssertTypeMismatchDecodingError(
                error,
                debugDescription: "Expected to decode String but found a boolean instead.",
                at: ["name"]
            )
        }
    }

    func testNestedTypeMismatch() {
        let smo: FbSmo = [
            "name": "John",
            "parent": [
                "name": 42,
            ],
        ]

        XCTAssertThrowsError(try FbSmoDecoder().decode(Child.self, from: smo)) { error in
            XCTAssertTypeMismatchDecodingError(
                error,
                debugDescription: "Expected to decode String but found an integer instead.",
                at: ["parent", "name"]
            )
        }
    }
}

private func XCTAssertTypeMismatchDecodingError(
    _ error: Error,
    debugDescription: String,
    at codingPath: [String]? = nil,
    file: StaticString = #file,
    line: UInt = #line
) {
    guard case DecodingError.typeMismatch(_, let context) = error else {
        XCTFail("Expected \(DecodingError.typeMismatch.self) but got: \(error)", file: file, line: line)
        return
    }

    XCTAssertEqual(context.debugDescription, debugDescription, file: file, line: line)

    if let codingPath = codingPath {
        let actualCodingPath = context.codingPath.map { $0.stringValue }
        XCTAssertEqual(actualCodingPath, codingPath, file: file, line: line)
    }
}

private struct Parent: Codable {
    let name: String
}

private struct Child: Codable {
    let name: String
    let parent: Parent?
}

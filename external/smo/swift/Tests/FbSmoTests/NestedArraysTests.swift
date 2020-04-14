import XCTest
@testable import FbSmo

class NestedArraysTests: XCTestCase {
    let expected = [[[[[1 as Int]]]]]

    /// Decode using JSONDecoder to check that Codable behaves as expected
    func testJsonReference() throws {
        let fixture = loadFixture("nested_arrays.json")
        let decoded = try JSONDecoder().decode(type(of: expected), from: fixture)
        XCTAssertEqual(decoded, expected)
    }

    func testJson() throws {
        let fixture = loadFixture("nested_arrays.json")
        let decoded = try FbSmoDecoder().decode(type(of: expected), from: fixture, using: .json)
        XCTAssertEqual(decoded, expected)
    }

    func testCbor() throws {
        let fixture = loadFixture("nested_arrays.cbor")
        let decoded = try FbSmoDecoder().decode(type(of: expected), from: fixture, using: .cbor)
        XCTAssertEqual(decoded, expected)
    }

    func testEncodeDecode() {
        encodeDecodeTest(expected)
    }
}

import XCTest
@testable import FbSmo

class MissingKeyTests: XCTestCase {
    func testMissingKey() {
        let smo = FbSmo.object([:])

        XCTAssertThrowsError(try FbSmoDecoder().decode(Child.self, from: smo)) { error in
            XCTAssertKeyNotFoundDecodingError(
                error,
                key: "name",
                debugDescription: "No value associated with key CodingKeys(stringValue: \"name\", intValue: nil) (\"name\").",
                at: []
            )
        }
    }
}

private func XCTAssertKeyNotFoundDecodingError(
    _ error: Error,
    key: String,
    debugDescription: String,
    at codingPath: [String]? = nil,
    file: StaticString = #file,
    line: UInt = #line
    ) {
    guard case DecodingError.keyNotFound(let actualKey, let context) = error else {
        XCTFail("Expected \(DecodingError.typeMismatch.self) but got: \(error)", file: file, line: line)
        return
    }

    XCTAssertEqual(actualKey.stringValue, key, file: file, line: line)
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

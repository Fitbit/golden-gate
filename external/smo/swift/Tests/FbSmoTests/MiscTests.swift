import XCTest
@testable import FbSmo

class MiscTests: XCTestCase {
    private let expected = Misc(
        tinyInt: 12,
        smallInt: 200,
        mediumInt: 80000,
        largeInt: 80000000000,
        array: [1, 2, 3, "1", "2", "3"],
        true: true,
        false: false,
        null: nil,
        tinyString: "tinyString",
        mediumString: "mediumString1 mediumString1 mediumString1 mediumString1 mediumString1",
        float: 1.2345,
        object: ["a": 1, "b": 2]
    )

    /// Decode using JSONDecoder to check that Codable behaves as expected
    func testJsonReference() throws {
        let fixture = loadFixture("misc.json")
        let decoded = try JSONDecoder().decode(Misc.self, from: fixture)
        XCTAssertEqual(decoded, expected)
    }

    func testJson() throws {
        let fixture = loadFixture("misc.json")
        let decoded = try FbSmoDecoder().decode(Misc.self, from: fixture, using: .json)
        XCTAssertEqual(decoded, expected)
    }

    func testCbor() throws {
        let fixture = loadFixture("misc.cbor")
        let decoded = try FbSmoDecoder().decode(Misc.self, from: fixture, using: .cbor)
        XCTAssertEqual(decoded, expected)
    }

    func testEncodeDecode() {
        encodeDecodeTest(expected)
    }
}

private struct Misc: Codable, Equatable {
    let tinyInt: Int
    let smallInt: Int
    let mediumInt: Int
    let largeInt: Int64
    let array: [NumberOrString]
    let `true`: Bool
    let `false`: Bool
    let null: Bool?
    let tinyString: String
    let mediumString: String
    let float: Double
    let object: [String: Int]
}

private enum NumberOrString: Equatable, Codable, ExpressibleByStringLiteral, ExpressibleByIntegerLiteral {
    case number(Int)
    case string(String)

    public init(extendedGraphemeClusterLiteral value: String) { self = .string(value) }
    public init(unicodeScalarLiteral value: String) { self = .string(value) }
    public init(stringLiteral value: String) { self = .string(value) }
    public init(integerLiteral value: Int) { self = .number(value) }

    init(from decoder: Decoder) throws {
        let container = try decoder.singleValueContainer()

        guard let number = try? container.decode(Int.self) else {
            self = .string(try container.decode(String.self))
            return
        }

        self = .number(number)
    }

    func encode(to encoder: Encoder) throws {
        var container = encoder.singleValueContainer()

        switch self {
        case .number(let number):
            try container.encode(number)
        case .string(let string):
            try container.encode(string)
        }
    }

    static func == (lhs: NumberOrString, rhs: NumberOrString) -> Bool {
        switch (lhs, rhs) {
        case let (.number(lhsNumber), .number(rhsNumber)):
            return lhsNumber == rhsNumber
        case (.number, _):
            return false
        case let (.string(lhsString), .string(rhsString)):
            return lhsString == rhsString
        case (.string, _):
            return false
        }
    }
}

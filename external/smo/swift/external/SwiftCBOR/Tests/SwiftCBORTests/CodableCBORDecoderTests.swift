import XCTest
@testable import SwiftCBOR

class CodableCBORDecoderTests: XCTestCase {
    static var allTests = [
        ("testDecodeNull", testDecodeNull),
        ("testDecodeBools", testDecodeBools),
        ("testDecodeInts", testDecodeInts),
        ("testDecodeNegativeInts", testDecodeNegativeInts),
        ("testDecodeStrings", testDecodeStrings),
        ("testDecodeByteStrings", testDecodeByteStrings),
        ("testDecodeArrays", testDecodeArrays),
        ("testDecodeMaps", testDecodeMaps),
        ("testDecodeDates", testDecodeDates)
    ]

    func testDecodeNull() {
        let decoded = try! CodableCBORDecoder().decode(Optional<String>.self, from: Data([0xf6]))
        XCTAssertNil(decoded)
    }

    func testDecodeBools() {
        let falseVal = try! CodableCBORDecoder().decode(Bool.self, from: Data([0xf4]))
        XCTAssertEqual(falseVal, false)
        let trueVal = try! CodableCBORDecoder().decode(Bool.self, from: Data([0xf5]))
        XCTAssertEqual(trueVal, true)
    }

    func testDecodeInts() {
        // Less than 24
        let zero = try! CodableCBORDecoder().decode(Int.self, from: Data([0x00]))
        XCTAssertEqual(zero, 0)
        let eight = try! CodableCBORDecoder().decode(Int.self, from: Data([0x08]))
        XCTAssertEqual(eight, 8)
        let ten = try! CodableCBORDecoder().decode(Int.self, from: Data([0x0a]))
        XCTAssertEqual(ten, 10)
        let twentyThree = try! CodableCBORDecoder().decode(Int.self, from: Data([0x17]))
        XCTAssertEqual(twentyThree, 23)

        // Just bigger than 23
        let twentyFour = try! CodableCBORDecoder().decode(Int.self, from: Data([0x18, 0x18]))
        XCTAssertEqual(twentyFour, 24)
        let twentyFive = try! CodableCBORDecoder().decode(Int.self, from: Data([0x18, 0x19]))
        XCTAssertEqual(twentyFive, 25)

        // Bigger
        let hundred = try! CodableCBORDecoder().decode(Int.self, from: Data([0x18, 0x64]))
        XCTAssertEqual(hundred, 100)
        let thousand = try! CodableCBORDecoder().decode(Int.self, from: Data([0x19, 0x03, 0xe8]))
        XCTAssertEqual(thousand, 1_000)
        let million = try! CodableCBORDecoder().decode(Int.self, from: Data([0x1a, 0x00, 0x0f, 0x42, 0x40]))
        XCTAssertEqual(million, 1_000_000)
        let trillion = try! CodableCBORDecoder().decode(Int.self, from: Data([0x1b, 0x00, 0x00, 0x00, 0xe8, 0xd4, 0xa5, 0x10, 0x00]))
        XCTAssertEqual(trillion, 1_000_000_000_000)

        // TODO: Tagged byte strings for big numbers
//        let bigNum = try! CodableCBORDecoder().decode(Int.self, from: Data([0x1b, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff]))
//        XCTAssertEqual(bigNum, 18_446_744_073_709_551_615)
//        let biggerNum = try! CodableCBORDecoder().decode(Int.self, from: Data([0x2c, 0x49, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,]))
//        XCTAssertEqual(biggerNum, 18_446_744_073_709_551_616)
    }

    func testDecodeNegativeInts() {
        // Less than 24
        let minusOne = try! CodableCBORDecoder().decode(Int.self, from: Data([0x20]))
        XCTAssertEqual(minusOne, -1)
        let minusTen = try! CodableCBORDecoder().decode(Int.self, from: Data([0x29]))
        XCTAssertEqual(minusTen, -10)

        // Bigger
        let minusHundred = try! CodableCBORDecoder().decode(Int.self, from: Data([0x38, 0x63]))
        XCTAssertEqual(minusHundred, -100)
        let minusThousand = try! CodableCBORDecoder().decode(Int.self, from: Data([0x39, 0x03, 0xe7]))
        XCTAssertEqual(minusThousand, -1_000)

        // TODO: Tagged byte strings for big numbers
//        let bigNum = try! CodableCBORDecoder().decode(Int.self, from: Data([0x1b, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff]))
//        XCTAssertEqual(bigNum, 18_446_744_073_709_551_615)
//        let biggerNum = try! CodableCBORDecoder().decode(Int.self, from: Data([0x2c, 0x49, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,]))
//        XCTAssertEqual(biggerNum, 18_446_744_073_709_551_616)
    }

    func testDecodeStrings() {
        let empty = try! CodableCBORDecoder().decode(String.self, from: Data([0x60]))
        XCTAssertEqual(empty, "")
        let a = try! CodableCBORDecoder().decode(String.self, from: Data([0x61, 0x61]))
        XCTAssertEqual(a, "a")
        let IETF = try! CodableCBORDecoder().decode(String.self, from: Data([0x64, 0x49, 0x45, 0x54, 0x46]))
        XCTAssertEqual(IETF, "IETF")
        let quoteSlash = try! CodableCBORDecoder().decode(String.self, from: Data([0x62, 0x22, 0x5c]))
        XCTAssertEqual(quoteSlash, "\"\\")
        let littleUWithDiaeresis = try! CodableCBORDecoder().decode(String.self, from: Data([0x62, 0xc3, 0xbc]))
        XCTAssertEqual(littleUWithDiaeresis, "\u{00FC}")

    }

    func testDecodeByteStrings() {
        let fourByteByteString = try! CodableCBORDecoder().decode(Data.self, from: Data([0x44, 0x01, 0x02, 0x03, 0x04]))
        XCTAssertEqual(fourByteByteString, Data([0x01, 0x02, 0x03, 0x04]))
        let indefiniteByteSting = try! CodableCBORDecoder().decode(Data.self, from: Data([0x5f, 0x42, 0x01, 0x02, 0x43, 0x03, 0x04, 0x05, 0xff]))
        XCTAssertEqual(indefiniteByteSting, Data([0x01, 0x02, 0x03, 0x04, 0x05]))
    }

    func testDecodeArrays() {
        let empty = try! CodableCBORDecoder().decode([String].self, from: Data([0x80]))
        XCTAssertEqual(empty, [])
        let oneTwoThree = try! CodableCBORDecoder().decode([Int].self, from: Data([0x83, 0x01, 0x02, 0x03]))
        XCTAssertEqual(oneTwoThree, [1, 2, 3])
        let lotsOfInts = try! CodableCBORDecoder().decode([Int].self, from: Data([0x98, 0x19, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x18, 0x18, 0x19]))
        XCTAssertEqual(lotsOfInts, [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25])
        let nestedSimple = try! CodableCBORDecoder().decode([[Int]].self, from: Data([0x83, 0x81, 0x01, 0x82, 0x02, 0x03, 0x82, 0x04, 0x05]))
        XCTAssertEqual(nestedSimple, [[1], [2, 3], [4, 5]])
        let indefiniteArray = try! CodableCBORDecoder().decode([Int].self, from: Data([0x9f, 0x04, 0x05, 0xff]))
        XCTAssertEqual(indefiniteArray, [4, 5])
        let nestedIndefiniteArrays = try! CodableCBORDecoder().decode([[Int]].self, from: Data([0x9f, 0x81, 0x01, 0x82, 0x02, 0x03, 0x9f, 0x04, 0x05, 0xff, 0xff]))
        XCTAssertEqual(nestedIndefiniteArrays, [[1], [2, 3], [4, 5]])
    }

    func testDecodeMaps() {
        let empty = try! CodableCBORDecoder().decode([String: String].self, from: Data([0xa0]))
        XCTAssertEqual(empty, [:])
        let stringToString = try! CodableCBORDecoder().decode([String: String].self, from: Data([0xa5, 0x61, 0x61, 0x61, 0x41, 0x61, 0x62, 0x61, 0x42, 0x61, 0x63, 0x61, 0x43, 0x61, 0x64, 0x61, 0x44, 0x61, 0x65, 0x61, 0x45]))
        XCTAssertEqual(stringToString, ["a": "A", "b": "B", "c": "C", "d": "D", "e": "E"])
        let oneTwoThreeFour = try! CodableCBORDecoder().decode([Int: Int].self, from: Data([0xa2, 0x01, 0x02, 0x03, 0x04]))
        XCTAssertEqual(oneTwoThreeFour, [1: 2, 3: 4])
        let indefiniteMap = try! CodableCBORDecoder().decode([String: String].self, from: Data([0xbf, 0x63, 0x46, 0x75, 0x6e, 0x61, 0x62, 0x63, 0x41, 0x6d, 0x74, 0x61, 0x63, 0xff]))
        XCTAssertEqual(indefiniteMap, ["Fun": "b", "Amt": "c"])
        let nestedIndefiniteMap = try! CodableCBORDecoder().decode([String: [String: String]].self, from: Data([0xbf, 0x63, 0x46, 0x75, 0x6e, 0xa1, 0x61, 0x62, 0x61, 0x42, 0x63, 0x41, 0x6d, 0x74, 0xbf, 0x61, 0x63, 0x61, 0x43, 0xff, 0xff]))
        XCTAssertEqual(nestedIndefiniteMap, ["Fun": ["b": "B"], "Amt": ["c": "C"]])
    }

    func testDecodeDates() {
        let expectedDateOne = Date(timeIntervalSince1970: 1363896240)
        let dateOne = try! CodableCBORDecoder().decode(Date.self, from: Data([0xc1, 0x1a, 0x51, 0x4b, 0x67, 0xb0]))
        XCTAssertEqual(dateOne, expectedDateOne)
        let expectedDateTwo = Date(timeIntervalSince1970: 1363896240.5)
        let dateTwo = try! CodableCBORDecoder().decode(Date.self, from: Data([0xc1, 0xfb, 0x41, 0xd4, 0x52, 0xd9, 0xec, 0x20, 0x00, 0x00]))
        XCTAssertEqual(dateTwo, expectedDateTwo)
    }
}

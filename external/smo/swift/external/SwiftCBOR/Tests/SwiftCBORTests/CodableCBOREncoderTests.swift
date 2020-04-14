import XCTest
@testable import SwiftCBOR

class CodableCBOREncoderTests: XCTestCase {
    static var allTests = [
        ("testEncodeNull", testEncodeNull),
        ("testEncodeBools", testEncodeBools),
        ("testEncodeInts", testEncodeInts),
        ("testEncodeNegativeInts", testEncodeNegativeInts),
        ("testEncodeStrings", testEncodeStrings),
        ("testEncodeByteStrings", testEncodeByteStrings),
        ("testEncodeArrays", testEncodeArrays),
        ("testEncodeMaps", testEncodeMaps),
        ("testEncodeSimpleStructs", testEncodeSimpleStructs)
    ]

    func testEncodeNull() {
        let encoded = try! CodableCBOREncoder().encode(Optional<String>(nil))
        XCTAssertEqual(encoded, Data([0xf6]))
    }

    func testEncodeBools() {
        let falseVal = try! CodableCBOREncoder().encode(false)
        XCTAssertEqual(falseVal, Data([0xf4]))
        let trueVal = try! CodableCBOREncoder().encode(true)
        XCTAssertEqual(trueVal, Data([0xf5]))
    }

    func testEncodeInts() {
        // Less than 24
        let zero = try! CodableCBOREncoder().encode(0)
        XCTAssertEqual(zero, Data([0x00]))
        let eight = try! CodableCBOREncoder().encode(8)
        XCTAssertEqual(eight, Data([0x08]))
        let ten = try! CodableCBOREncoder().encode(10)
        XCTAssertEqual(ten, Data([0x0a]))
        let twentyThree = try! CodableCBOREncoder().encode(23)
        XCTAssertEqual(twentyThree, Data([0x17]))

        // Just bigger than 23
        let twentyFour = try! CodableCBOREncoder().encode(24)
        XCTAssertEqual(twentyFour, Data([0x18, 0x18]))
        let twentyFive = try! CodableCBOREncoder().encode(25)
        XCTAssertEqual(twentyFive, Data([0x18, 0x19]))

        // Bigger
        let hundred = try! CodableCBOREncoder().encode(100)
        XCTAssertEqual(hundred, Data([0x18, 0x64]))
        let thousand = try! CodableCBOREncoder().encode(1_000)
        XCTAssertEqual(thousand, Data([0x19, 0x03, 0xe8]))
        let million = try! CodableCBOREncoder().encode(1_000_000)
        XCTAssertEqual(million, Data([0x1a, 0x00, 0x0f, 0x42, 0x40]))
        let trillion = try! CodableCBOREncoder().encode(1_000_000_000_000)
        XCTAssertEqual(trillion, Data([0x1b, 0x00, 0x00, 0x00, 0xe8, 0xd4, 0xa5, 0x10, 0x00]))

        // TODO: Tagged byte strings for big numbers
//        let bigNum = try! CodableCBORDecoder().decode(Int.self, from: Data([0x1b, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff]))
//        XCTAssertEqual(bigNum, 18_446_744_073_709_551_615)
//        let biggerNum = try! CodableCBORDecoder().decode(Int.self, from: Data([0x2c, 0x49, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,]))
//        XCTAssertEqual(biggerNum, 18_446_744_073_709_551_616)
    }

    func testEncodeNegativeInts() {
        // Less than 24
        let minusOne = try! CodableCBOREncoder().encode(-1)
        XCTAssertEqual(minusOne, Data([0x20]))
        let minusTen = try! CodableCBOREncoder().encode(-10)
        XCTAssertEqual(minusTen, Data([0x29]))

        // Bigger
        let minusHundred = try! CodableCBOREncoder().encode(-100)
        XCTAssertEqual(minusHundred, Data([0x38, 0x63]))
        let minusThousand = try! CodableCBOREncoder().encode(-1_000)
        XCTAssertEqual(minusThousand, Data([0x39, 0x03, 0xe7]))

        // TODO: Tagged byte strings for big negative numbers
//        let bigNum = try! CodableCBORDecoder().decode(Int.self, from: Data([0x1b, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff]))
//        XCTAssertEqual(bigNum, 18_446_744_073_709_551_615)
//        let biggerNum = try! CodableCBORDecoder().decode(Int.self, from: Data([0x2c, 0x49, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,]))
//        XCTAssertEqual(biggerNum, 18_446_744_073_709_551_616)
    }

    func testEncodeStrings() {
        let empty = try! CodableCBOREncoder().encode("")
        XCTAssertEqual(empty, Data([0x60]))
        let a = try! CodableCBOREncoder().encode("a")
        XCTAssertEqual(a, Data([0x61, 0x61]))
        let IETF = try! CodableCBOREncoder().encode("IETF")
        XCTAssertEqual(IETF, Data([0x64, 0x49, 0x45, 0x54, 0x46]))
        let quoteSlash = try! CodableCBOREncoder().encode("\"\\")
        XCTAssertEqual(quoteSlash, Data([0x62, 0x22, 0x5c]))
        let littleUWithDiaeresis = try! CodableCBOREncoder().encode("\u{00FC}")
        XCTAssertEqual(littleUWithDiaeresis, Data([0x62, 0xc3, 0xbc]))
    }

    func testEncodeByteStrings() {
        let fourByteByteString = try! CodableCBOREncoder().encode(Data([0x01, 0x02, 0x03, 0x04]))
        XCTAssertEqual(fourByteByteString, Data([0x44, 0x01, 0x02, 0x03, 0x04]))
    }

    func testEncodeArrays() {
        let empty = try! CodableCBOREncoder().encode([String]())
        XCTAssertEqual(empty, Data([0x80]))
        let oneTwoThree = try! CodableCBOREncoder().encode([1, 2, 3])
        XCTAssertEqual(oneTwoThree, Data([0x83, 0x01, 0x02, 0x03]))
        let lotsOfInts = try! CodableCBOREncoder().encode([1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25])
        XCTAssertEqual(lotsOfInts, Data([0x98, 0x19, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x18, 0x18, 0x19]))
        let nestedSimple = try! CodableCBOREncoder().encode([[1], [2, 3], [4, 5]])
        XCTAssertEqual(nestedSimple, Data([0x83, 0x81, 0x01, 0x82, 0x02, 0x03, 0x82, 0x04, 0x05]))
    }

    func testEncodeMaps() {
        let empty = try! CodableCBOREncoder().encode([String: String]())
        XCTAssertEqual(empty, Data([0xa0]))
        let stringToString = try! CodableCBOREncoder().encode(["a": "A", "b": "B", "c": "C", "d": "D", "e": "E"])
        XCTAssertEqual(stringToString.first!, 0xa5)
        let dataMinusFirstByte = stringToString[1...].map { $0 }.chunked(into: 4).sorted(by: { $0.lexicographicallyPrecedes($1) })
        let dataForKeyValuePairs: [[UInt8]] = [[0x61, 0x61, 0x61, 0x41], [0x61, 0x62, 0x61, 0x42], [0x61, 0x63, 0x61, 0x43], [0x61, 0x64, 0x61, 0x44], [0x61, 0x65, 0x61, 0x45]]
        XCTAssertEqual(dataMinusFirstByte, dataForKeyValuePairs)
        let oneTwoThreeFour = try! CodableCBOREncoder().encode([1: 2, 3: 4])
        XCTAssert(
            oneTwoThreeFour == Data([0xa2, 0x01, 0x02, 0x03, 0x04])
            || oneTwoThreeFour == Data([0xa2, 0x03, 0x04, 0x01, 0x02])
        )
    }

    func testEncodeSimpleStructs() {
        struct MyStruct: Codable {
            let age: Int
            let name: String
        }

        let encoded = try! CodableCBOREncoder().encode(MyStruct(age: 27, name: "Ham")).map { $0 }

        XCTAssert(
            encoded == [0xa2, 0x63, 0x61, 0x67, 0x65, 0x18, 0x1b, 0x64, 0x6e, 0x61, 0x6d, 0x65, 0x63, 0x48, 0x61, 0x6d]
            || encoded == [0xa2, 0x64, 0x6e, 0x61, 0x6d, 0x65, 0x63, 0x48, 0x61, 0x6d, 0x63, 0x61, 0x67, 0x65, 0x18, 0x1b]
        )
    }
}

extension Array {
    func chunked(into size: Int) -> [[Element]] {
        return stride(from: 0, to: count, by: size).map {
            Array(self[$0 ..< Swift.min($0 + size, count)])
        }
    }
}

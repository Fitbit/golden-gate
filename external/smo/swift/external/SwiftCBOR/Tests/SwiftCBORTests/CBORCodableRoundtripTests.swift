import XCTest
@testable import SwiftCBOR

class CBORCodableRoundtripTests: XCTestCase {
    static var allTests = [
        ("testSimpleStruct", testSimpleStruct),
        ("testSimpleStructsInArray", testSimpleStructsInArray),
        ("testSimpleStructsAsValuesInMap", testSimpleStructsAsValuesInMap),
        ("testSimpleStructsAsKeysInMap", testSimpleStructsAsKeysInMap),
        ("testNil", testNil),
        ("testBools", testBools),
        ("testInts", testInts),
        ("testNegativeInts", testNegativeInts),
        ("testStrings", testStrings),
        ("testArrays", testArrays),
        ("testMaps", testMaps),
        ("testWrappedStruct", testWrappedStruct),
    ]

    struct MyStruct: Codable, Equatable, Hashable {
        let age: Int
        let name: String
    }

    func testSimpleStruct() {
        let encoded = try! CodableCBOREncoder().encode(MyStruct(age: 27, name: "Ham"))
        let decoded = try! CodableCBORDecoder().decode(MyStruct.self, from: encoded)
        XCTAssertEqual(decoded, MyStruct(age: 27, name: "Ham"))
    }

    func testSimpleStructsInArray() {
        let encoded = try! CodableCBOREncoder().encode([
            MyStruct(age: 27, name: "Ham"),
            MyStruct(age: 24, name: "Greg")
        ])
        let decoded = try! CodableCBORDecoder().decode([MyStruct].self, from: encoded)
        XCTAssertEqual(decoded, [MyStruct(age: 27, name: "Ham"), MyStruct(age: 24, name: "Greg")])
    }

    func testSimpleStructsAsValuesInMap() {
        let encoded = try! CodableCBOREncoder().encode([
            "Ham": MyStruct(age: 27, name: "Ham"),
            "Greg": MyStruct(age: 24, name: "Greg")
        ])
        let decoded = try! CodableCBORDecoder().decode([String: MyStruct].self, from: encoded)
        XCTAssertEqual(
            decoded,
            [
                "Ham": MyStruct(age: 27, name: "Ham"),
                "Greg": MyStruct(age: 24, name: "Greg")
            ]
        )
    }

    func testSimpleStructsAsKeysInMap() {
        let encoded = try! CodableCBOREncoder().encode([
            MyStruct(age: 27, name: "Ham"): "Ham",
            MyStruct(age: 24, name: "Greg"): "Greg"
        ])
        let decoded = try! CodableCBORDecoder().decode([MyStruct: String].self, from: encoded)
        XCTAssertEqual(
            decoded,
            [
                MyStruct(age: 27, name: "Ham"): "Ham",
                MyStruct(age: 24, name: "Greg"): "Greg"
            ]
        )
    }

    func testNil() {
        let nilValue = try! CodableCBOREncoder().encode(Optional<String>(nil))
        let nilDecoded = try! CodableCBORDecoder().decode(Optional<String>.self, from: nilValue)
        XCTAssertNil(nilDecoded)
    }

    func testBools() {
        let falseVal = try! CodableCBOREncoder().encode(false)
        let falseValDecoded = try! CodableCBORDecoder().decode(Bool.self, from: falseVal)
        XCTAssertFalse(falseValDecoded)
        let trueVal = try! CodableCBOREncoder().encode(true)
        let trueValDecoded = try! CodableCBORDecoder().decode(Bool.self, from: trueVal)
        XCTAssertTrue(trueValDecoded)
    }

    func testInts() {
        // Less than 24
        let zero = try! CodableCBOREncoder().encode(0)
        let zeroDecoded = try! CodableCBORDecoder().decode(Int.self, from: zero)
        XCTAssertEqual(zeroDecoded, 0)
        let eight = try! CodableCBOREncoder().encode(8)
        let eightDecoded = try! CodableCBORDecoder().decode(Int.self, from: eight)
        XCTAssertEqual(eightDecoded, 8)
        let ten = try! CodableCBOREncoder().encode(10)
        let tenDecoded = try! CodableCBORDecoder().decode(Int.self, from: ten)
        XCTAssertEqual(tenDecoded, 10)
        let twentyThree = try! CodableCBOREncoder().encode(23)
        let twentyThreeDecoded = try! CodableCBORDecoder().decode(Int.self, from: twentyThree)
        XCTAssertEqual(twentyThreeDecoded, 23)

        // Just bigger than 23
        let twentyFour = try! CodableCBOREncoder().encode(24)
        let twentyFourDecoded = try! CodableCBORDecoder().decode(Int.self, from: twentyFour)
        XCTAssertEqual(twentyFourDecoded, 24)
        let twentyFive = try! CodableCBOREncoder().encode(25)
        let twentyFiveDecoded = try! CodableCBORDecoder().decode(Int.self, from: twentyFive)
        XCTAssertEqual(twentyFiveDecoded, 25)

        // Bigger
        let hundred = try! CodableCBOREncoder().encode(100)
        let hundredDecoded = try! CodableCBORDecoder().decode(Int.self, from: hundred)
        XCTAssertEqual(hundredDecoded, 100)
        let thousand = try! CodableCBOREncoder().encode(1_000)
        let thousandDecoded = try! CodableCBORDecoder().decode(Int.self, from: thousand)
        XCTAssertEqual(thousandDecoded, 1_000)
        let million = try! CodableCBOREncoder().encode(1_000_000)
        let millionDecoded = try! CodableCBORDecoder().decode(Int.self, from: million)
        XCTAssertEqual(millionDecoded, 1_000_000)
        let trillion = try! CodableCBOREncoder().encode(1_000_000_000_000)
        let trillionDecoded = try! CodableCBORDecoder().decode(Int.self, from: trillion)
        XCTAssertEqual(trillionDecoded, 1_000_000_000_000)

        // TODO: Tagged byte strings for big numbers
//        let bigNum = try! CodableCBORDecoder().decode(Int.self, from: Data([0x1b, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff]))
//        XCTAssertEqual(bigNum, 18_446_744_073_709_551_615)
//        let biggerNum = try! CodableCBORDecoder().decode(Int.self, from: Data([0x2c, 0x49, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,]))
//        XCTAssertEqual(biggerNum, 18_446_744_073_709_551_616)
    }

    func testNegativeInts() {
        // Less than 24
        let minusOne = try! CodableCBOREncoder().encode(-1)
        let minusOneDecoded = try! CodableCBORDecoder().decode(Int.self, from: minusOne)
        XCTAssertEqual(minusOneDecoded, -1)
        let minusTen = try! CodableCBOREncoder().encode(-10)
        let minusTenDecoded = try! CodableCBORDecoder().decode(Int.self, from: minusTen)
        XCTAssertEqual(minusTenDecoded, -10)

        // Bigger
        let minusHundred = try! CodableCBOREncoder().encode(-100)
        let minusHundredDecoded = try! CodableCBORDecoder().decode(Int.self, from: minusHundred)
        XCTAssertEqual(minusHundredDecoded, -100)
        let minusThousand = try! CodableCBOREncoder().encode(-1_000)
        let minusThousandDecoded = try! CodableCBORDecoder().decode(Int.self, from: minusThousand)
        XCTAssertEqual(minusThousandDecoded, -1_000)

        // TODO: Tagged byte strings for big numbers
//        let bigNum = try! CodableCBORDecoder().decode(Int.self, from: Data([0x1b, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff]))
//        XCTAssertEqual(bigNum, 18_446_744_073_709_551_615)
//        let biggerNum = try! CodableCBORDecoder().decode(Int.self, from: Data([0x2c, 0x49, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,]))
//        XCTAssertEqual(biggerNum, 18_446_744_073_709_551_616)
    }

    func testStrings() {
        let empty = try! CodableCBOREncoder().encode("")
        let emptyDecoded = try! CodableCBORDecoder().decode(String.self, from: empty)
        XCTAssertEqual(emptyDecoded, "")
        let a = try! CodableCBOREncoder().encode("a")
        let aDecoded = try! CodableCBORDecoder().decode(String.self, from: a)
        XCTAssertEqual(aDecoded, "a")
        let IETF = try! CodableCBOREncoder().encode("IETF")
        let IETFDecoded = try! CodableCBORDecoder().decode(String.self, from: IETF)
        XCTAssertEqual(IETFDecoded, "IETF")
        let quoteSlash = try! CodableCBOREncoder().encode("\"\\")
        let quoteSlashDecoded = try! CodableCBORDecoder().decode(String.self, from: quoteSlash)
        XCTAssertEqual(quoteSlashDecoded, "\"\\")
        let littleUWithDiaeresis = try! CodableCBOREncoder().encode("\u{00FC}")
        let littleUWithDiaeresisDecoded = try! CodableCBORDecoder().decode(String.self, from: littleUWithDiaeresis)
        XCTAssertEqual(littleUWithDiaeresisDecoded, "\u{00FC}")
    }

    func testArrays() {
        let empty = try! CodableCBOREncoder().encode([String]())
        let emptyDecoded = try! CodableCBORDecoder().decode([String].self, from: empty)
        XCTAssertEqual(emptyDecoded, [])
        let oneTwoThree = try! CodableCBOREncoder().encode([1, 2, 3])
        let oneTwoThreeDecoded = try! CodableCBORDecoder().decode([Int].self, from: oneTwoThree)
        XCTAssertEqual(oneTwoThreeDecoded, [1, 2, 3])
        let lotsOfInts = try! CodableCBOREncoder().encode([1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25])
        let lotsOfIntsDecoded = try! CodableCBORDecoder().decode([Int].self, from: lotsOfInts)
        XCTAssertEqual(lotsOfIntsDecoded, [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25])
        let nestedSimple = try! CodableCBOREncoder().encode([[1], [2, 3], [4, 5]])
        let nestedSimpleDecoded = try! CodableCBORDecoder().decode([[Int]].self, from: nestedSimple)
        XCTAssertEqual(nestedSimpleDecoded, [[1], [2, 3], [4, 5]])
    }

    func testMaps() {
        let empty = try! CodableCBOREncoder().encode([String: String]())
        let emptyDecoded = try! CodableCBORDecoder().decode([String: String].self, from: empty)
        XCTAssertEqual(emptyDecoded, [:])
        let stringToString = try! CodableCBOREncoder().encode(["a": "A", "b": "B", "c": "C", "d": "D", "e": "E"])
        let stringToStringDecoded = try! CodableCBORDecoder().decode([String: String].self, from: stringToString)
        XCTAssertEqual(stringToStringDecoded, ["a": "A", "b": "B", "c": "C", "d": "D", "e": "E"])
        let oneTwoThreeFour = try! CodableCBOREncoder().encode([1: 2, 3: 4])
        let oneTwoThreeFourDecoded = try! CodableCBORDecoder().decode([Int: Int].self, from: oneTwoThreeFour)
        XCTAssertEqual(oneTwoThreeFourDecoded, [1: 2, 3: 4])
    }

    func testWrappedStruct() {
        struct Wrapped<T: Codable>: Decodable {
            let _id: String
            let value: T

            private enum CodingKeys: String, CodingKey {
                case _id
            }

            init(from decoder: Decoder) throws {
                let container = try decoder.container(keyedBy: CodingKeys.self)

                _id = try container.decode(String.self, forKey: ._id)
                value = try T(from: decoder)
            }
        }

        struct BasicCar: Codable {
            let color: String
            let age: Int
            let data: Data
        }

        struct Car: Codable {
            let _id: String
            let color: String
            let age: Int
            let data: Data
        }

        // Generate some random Data
        let randomBytes = (1...4).map { _ in UInt8.random(in: 0...UInt8.max) }
        let data = Data(randomBytes)

        let car = Car(
            _id: "5caf23633337661721236cfa",
            color: "Red",
            age: 56,
            data: data
        )

        let encodedCar = try! CodableCBOREncoder().encode(car)
        let decoded = try! CodableCBORDecoder().decode(Wrapped<BasicCar>.self, from: encodedCar)

        XCTAssertEqual(decoded._id, car._id)
        XCTAssertEqual(decoded.value.color, car.color)
        XCTAssertEqual(decoded.value.age, car.age)
        XCTAssertEqual(decoded.value.data, data)
    }
}

import XCTest
@testable import SwiftCBOR

class CBOREncoderTests: XCTestCase {
    static var allTests = [
        ("testEncodeInts", testEncodeInts),
        ("testEncodeByteStrings", testEncodeByteStrings),
        ("testEncodeData", testEncodeData),
        ("testEncodeUtf8Strings", testEncodeUtf8Strings),
        ("testEncodeArrays", testEncodeArrays),
        ("testEncodeMaps", testEncodeMaps),
        ("testEncodeTagged", testEncodeTagged),
        ("testEncodeSimple", testEncodeSimple),
        ("testEncodeFloats", testEncodeFloats),
        ("testEncodeIndefiniteArrays", testEncodeIndefiniteArrays),
        ("testEncodeIndefiniteMaps", testEncodeIndefiniteMaps),
        ("testEncodeIndefiniteStrings", testEncodeIndefiniteStrings),
        ("testEncodeIndefiniteByteStrings", testEncodeIndefiniteByteStrings),
        ("testEncodeDates", testEncodeDates),
        ("testReadmeExamples", testReadmeExamples),
    ]

    func assertEquivalent<T: CBOREncodable>(_ input: T, _ cbor: [UInt8]) {
        XCTAssertEqual(input.encode(), cbor)
        XCTAssertEqual(try! CBOR.decode(input.encode()), try! CBOR.decode(cbor))
    }

    func testEncodeInts() {
        for i in 0..<24 {
            assertEquivalent(i, [UInt8(i)])
            XCTAssertEqual((-i).encode().count, 1)
        }
        XCTAssertEqual(Int(-1).encode(), [0x20])
        XCTAssertEqual(CBOR.encode(-10), [0x29])
        XCTAssertEqual(Int(-24).encode(), [0x37])
        XCTAssertEqual(Int(-25).encode(), [0x38, 24])
        XCTAssertEqual(1000000.encode(), [0x1a, 0x00, 0x0f, 0x42, 0x40])
        XCTAssertEqual(4294967295.encode(), [0x1a, 0xff, 0xff, 0xff, 0xff]) //UInt32.max
        XCTAssertEqual(1000000000000.encode(), [0x1b, 0x00, 0x00, 0x00, 0xe8, 0xd4, 0xa5, 0x10, 0x00])
        XCTAssertEqual(Int(-1_000_000).encode(), [0x3a, 0x00, 0x0f, 0x42, 0x3f])
        XCTAssertEqual(Int(-1_000_000_000_000).encode(), [0x3b, 0x00, 0x00, 0x00, 0xe8, 0xd4, 0xa5, 0x0f, 0xff])
    }

    func testEncodeByteStrings() {
        XCTAssertEqual(CBOR.encode([UInt8](), asByteString: true), [0x40])
        XCTAssertEqual(CBOR.encode([UInt8]([0xf0]), asByteString: true), [0x41, 0xf0])
        XCTAssertEqual(CBOR.encode([UInt8]([0x01, 0x02, 0x03, 0x04]), asByteString: true), [0x44, 0x01, 0x02, 0x03, 0x04])
        let bs23: [UInt8] = [0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xaa]
        assert(bs23.count == 23)
        XCTAssertEqual(CBOR.encode(bs23, asByteString: true), [0x57] + bs23)
        let bs24 = bs23 + [0xaa]
        XCTAssertEqual(CBOR.encode(bs24, asByteString: true), [0x58, 24] + bs24)

        // non-UInt8, raw bytes (reversed).
        XCTAssertEqual(CBOR.encode([UInt16]([240]), asByteString: true), [0x41, 0x00, 0xf0])
    }

    func testEncodeData() {
        XCTAssertEqual(CBOR.encode(Data()), [0x40])
        XCTAssertEqual(CBOR.encode(Data([0xf0])), [0x41, 0xf0])
        XCTAssertEqual(CBOR.encode(Data([0x01, 0x02, 0x03, 0x04])), [0x44, 0x01, 0x02, 0x03, 0x04])
        let bs23 = Data([0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xaa])
        assert(bs23.count == 23)
        XCTAssertEqual(CBOR.encode(bs23), [0x57] + bs23)
        let bs24 = Data(bs23 + [0xaa])
        XCTAssertEqual(CBOR.encode(bs24), [0x58, 24] + bs24)

        // non-UInt8, raw bytes (reversed).
        XCTAssertEqual(CBOR.encode([UInt16]([240]), asByteString: true), [0x41, 0x00, 0xf0])
    }

    func testEncodeUtf8Strings() {
        XCTAssertEqual("".encode(), [0x60])
        XCTAssertEqual("a".encode(), [0x61, 0x61])
        XCTAssertEqual("B".encode(), [0x61, 0x42])
        XCTAssertEqual(try! CBOR.decode("B".encode()), try! CBOR.decode([0x78, 1, 0x42]))
        XCTAssertEqual("ABC".encode(), [0x63, 0x41, 0x42, 0x43])
        XCTAssertEqual(try! CBOR.decode("ABC".encode()), try! CBOR.decode([0x79, 0x00, 3, 0x41, 0x42, 0x43]))
        XCTAssertEqual("IETF".encode(), [0x64, 0x49, 0x45, 0x54, 0x46])
        XCTAssertEqual("今日は".encode(), [0x69, 0xE4, 0xBB, 0x8A, 0xE6, 0x97, 0xA5, 0xE3, 0x81, 0xAF])
        XCTAssertEqual("♨️français;日本語！Longer text\n with break?".encode(), [0x78, 0x34, 0xe2, 0x99, 0xa8, 0xef, 0xb8, 0x8f, 0x66, 0x72, 0x61, 0x6e, 0xc3, 0xa7, 0x61, 0x69, 0x73, 0x3b, 0xe6, 0x97, 0xa5, 0xe6, 0x9c, 0xac, 0xe8, 0xaa, 0x9e, 0xef, 0xbc, 0x81, 0x4c, 0x6f, 0x6e, 0x67, 0x65, 0x72, 0x20, 0x74, 0x65, 0x78, 0x74, 0x0a, 0x20, 0x77, 0x69, 0x74, 0x68, 0x20, 0x62, 0x72, 0x65, 0x61, 0x6b, 0x3f])
        XCTAssertEqual("\"\\".encode(), [0x62, 0x22, 0x5c])
        XCTAssertEqual("\u{6C34}".encode(), [0x63, 0xe6, 0xb0, 0xb4])
        XCTAssertEqual("水".encode(), [0x63, 0xe6, 0xb0, 0xb4])
        XCTAssertEqual("\u{00fc}".encode(), [0x62, 0xc3, 0xbc])
        XCTAssertEqual("abc\n123".encode(), [0x67, 0x61, 0x62, 0x63, 0x0a, 0x31, 0x32, 0x33])
    }

    func testEncodeArrays() {
        XCTAssertEqual(CBOR.encode(Array<Int>()), [0x80])
        XCTAssertEqual(Array<Int>().encode(), [0x80])

        XCTAssertEqual(CBOR.encode([1, 2, 3]), [0x83, 0x01, 0x02, 0x03])
        XCTAssertEqual([1, 2, 3].encode(), [0x83, 0x01, 0x02, 0x03])

        let arr: [[UInt64]] = [[1], [2, 3], [4, 5]]

        let wrapped = arr.map{ inner in return CBOR.array(inner.map{ return CBOR.unsignedInt($0) })}
        XCTAssertEqual(CBOR.encode(wrapped), [0x83, 0x81, 0x01, 0x82, 0x02, 0x03, 0x82, 0x04, 0x05])

        let arr25Enc: [UInt8] = [0x98,0x19,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x18,0x18,0x19]
        XCTAssertEqual(CBOR.encode(Array<Int>(1...25)), arr25Enc)
    }

    func testEncodeMaps() {
        XCTAssertEqual(CBOR.encode(Dictionary<Int, Int>()), [0xa0])

        let encoded = CBOR.encode([1: 2, 3: 4])
        XCTAssert(encoded == [0xa2, 0x01, 0x02, 0x03, 0x04] || encoded == [0xa2, 0x03, 0x04, 0x01, 0x02])

        let arr1: CBOR = [1]
        let arr2: CBOR = [2,3]
        let nestedEnc: [UInt8] = CBOR.encode(["a": arr1, "b": arr2])
        let encodedAFirst: [UInt8] = [0xa2, 0x61, 0x61, 0x81, 0x01, 0x61, 0x62, 0x82, 0x02, 0x03]
        let encodedBFirst: [UInt8] = [0xa2, 0x61, 0x62, 0x82, 0x02, 0x03, 0x61, 0x61, 0x81, 0x01]
        XCTAssert(nestedEnc == encodedAFirst || nestedEnc == encodedBFirst)

        let mapToAny: [String: Any] = [
            "a": 1,
            "b": [2, 3]
        ]
        let encodedMapToAny = try! CBOR.encodeMap(mapToAny)
        XCTAssertEqual(encodedMapToAny, [0xa2, 0x61, 0x61, 0x01, 0x61, 0x62, 0x82, 0x02, 0x03])
    }

    func testEncodeTagged() {
        let bignum: [UInt8] = [0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00] // 2**64
        let bignumCBOR = CBOR.byteString(bignum)
        XCTAssertEqual(CBOR.encodeTagged(tag: .positiveBignum, value: bignumCBOR), [0xc2, 0x49] + bignum)
        XCTAssertEqual(CBOR.encodeTagged(tag: .init(rawValue: UInt64.max), value: bignumCBOR), [0xdb, 255, 255, 255, 255, 255, 255, 255, 255, 0x49] + bignum)
    }

    func testEncodeSimple() {
        XCTAssertEqual(false.encode(), [0xf4])
        XCTAssertEqual(true.encode(), [0xf5])
        XCTAssertEqual(CBOR.null.encode(), [0xf6])
        XCTAssertEqual(CBOR.undefined.encode(), [0xf7])
        XCTAssertEqual(CBOR.simple(16).encode(), [0xf0])
        XCTAssertEqual(CBOR.simple(24).encode(), [0xf8, 0x18])
        XCTAssertEqual(CBOR.simple(255).encode(), [0xf8, 0xff])
        XCTAssertEqual(CBOR.break.encode(), [0xff])
    }

    func testEncodeFloats() {
        // The following tests are modifications of examples of Float16 in the RFC
        XCTAssertEqual(Float(0.0).encode(), [0xfa,0x00, 0x00, 0x00, 0x00])
        XCTAssertEqual(Float(-0.0).encode(), [0xfa, 0x80, 0x00, 0x00, 0x00])
        XCTAssertEqual(Float(1.0).encode(), [0xfa, 0x3f, 0x80, 0x00, 0x00])
        XCTAssertEqual(Float(1.5).encode(), [0xfa,0x3f,0xc0, 0x00,0x00])
        XCTAssertEqual(Float(65504.0).encode(), [0xfa, 0x47, 0x7f, 0xe0, 0x00])

        // The following are seen as Float32s in the RFC
        XCTAssertEqual(Float(100000.0).encode(), [0xfa,0x47,0xc3,0x50,0x00])
        XCTAssertEqual(Float(3.4028234663852886e+38).encode(), [0xfa, 0x7f, 0x7f, 0xff, 0xff])

        // The following are seen as Doubles in the RFC
        XCTAssertEqual(Double(1.1).encode(), [0xfb,0x3f,0xf1,0x99,0x99,0x99,0x99,0x99,0x9a])
        XCTAssertEqual(Double(-4.1).encode(), [0xfb, 0xc0, 0x10, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66])
        XCTAssertEqual(CBOR.encode(1.0e+300), [0xfb, 0x7e, 0x37, 0xe4, 0x3c, 0x88, 0x00, 0x75, 0x9c])
        XCTAssertEqual(Double(5.960464477539063e-8).encode(), [0xfb, 0x3e, 0x70, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00])

        // Special values
        XCTAssertEqual(Float.infinity.encode(), [0xfa, 0x7f, 0x80, 0x00, 0x00])
        XCTAssertEqual(CBOR.encode(-Float.infinity), [0xfa, 0xff, 0x80, 0x00, 0x00])
        XCTAssertEqual(Double.infinity.encode(), [0xfb, 0x7f, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00])
        XCTAssertEqual(CBOR.encode(-Double.infinity), [0xfb, 0xff, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00])
        XCTAssertEqual(Float.nan.encode(), [0xfa,0x7f, 0xc0, 0x00, 0x00])
        XCTAssertEqual(Double.nan.encode(), [0xfb, 0x7f, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00])

        // Swift's floating point literals are read as Doubles unless specifically specified. e.g.
        XCTAssertEqual(CBOR.encode(0.0), [0xfb,0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00])
    }

    func testEncodeIndefiniteArrays() {
        var indefArray = CBOR.encodeArrayStreamStart()
        XCTAssertEqual(indefArray, [0x9f])
        let a: [Int] = [1, 2]
        indefArray.append(contentsOf: CBOR.encodeArrayChunk(a)) // this encode a definite array!
        // send current indef array (without close. the receiver would be programmed to anticipate this.
        indefArray.append(contentsOf: 3.encode())
        indefArray.append(contentsOf: CBOR.encodeStreamEnd())
        XCTAssertEqual(indefArray, [0x9f, 0x01, 0x02, 0x03, 0xff])

        indefArray.removeLast() // remove stream end
        let innerArray = [UInt8]([1,2,3])
        indefArray.append(contentsOf: CBOR.encodeArrayStreamStart() + CBOR.encodeArrayChunk(innerArray))
        indefArray.append(contentsOf: CBOR.encodeStreamEnd()) // close inner array
        indefArray.append(contentsOf: CBOR.encodeStreamEnd()) // close outer
        XCTAssertEqual(indefArray, [0x9f, 0x01, 0x02, 0x03, 0x9f, 0x01, 0x02, 0x03, 0xff, 0xff])
    }

    func testEncodeIndefiniteMaps() {
        XCTAssertEqual(CBOR.encodeMapStreamStart(), [0xbf])
        let map: [String: Int] = ["a": 1]
        let map2 = ["B": 2]
        let a1_enc: [UInt8] = [0x61, 0x61, 0x01]
        let b2_enc: [UInt8] = [0x61, 0x42, 0x02]
        let final: [UInt8] = CBOR.encodeMapStreamStart() + CBOR.encodeMapChunk(map) + CBOR.encodeMapChunk(map2) + CBOR.encodeStreamEnd()
        XCTAssertEqual(final, [0xbf] + a1_enc + b2_enc + [0xff])
    }

    func testEncodeIndefiniteStrings() {
        XCTAssertEqual(CBOR.encodeStringStreamStart(), [0x7f])
        let s = "a"
        let t = "B"
        let st_enc: [UInt8] = [0x7f, 0x61, 0x61, 0x61, 0x42, 0xff]
        XCTAssertEqual(CBOR.encodeStringStreamStart() + CBOR.encode(s) + CBOR.encode(t) + CBOR.encodeStreamEnd(), st_enc)
    }

    func testEncodeIndefiniteByteStrings() {
        XCTAssertEqual(CBOR.encodeByteStringStreamStart(), [0x5f])
        let bs: [UInt8] = [0xf0]
        let bs2: [UInt8] = [0xff]
        let cbor: [UInt8] = [0x5f, 0x41, 0xf0, 0x41, 0xff, 0xff]
        let encoded = CBOR.encodeByteStringStreamStart()
            + CBOR.encode(bs, asByteString: true)
            + CBOR.encode(bs2, asByteString: true)
            + CBOR.encodeStreamEnd()
        XCTAssertEqual(encoded, cbor)
    }

    func testEncodeDates() {
        let dateFormatter = DateFormatter()
        dateFormatter.dateFormat = "yyyy-MM-dd'T'HH:mm:ssZ"
        dateFormatter.locale = Locale(identifier: "en_US_POSIX")
        dateFormatter.timeZone = TimeZone(secondsFromGMT: 0)
        let dateOne = dateFormatter.date(from: "2013-03-21T20:04:00+00:00")!
        XCTAssertEqual(CBOR.encodeDate(dateOne), [0xc1, 0x1a, 0x51, 0x4b, 0x67, 0xb0])

        let dateTwo = Date(timeIntervalSince1970: 1363896240.5)
        XCTAssertEqual(CBOR.encodeDate(dateTwo), [0xc1, 0xfb, 0x41, 0xd4, 0x52, 0xd9, 0xec, 0x20, 0x00, 0x00])
    }

    func testReadmeExamples() {
        XCTAssertEqual(100.encode(),     [0x18, 0x64])
        XCTAssertEqual(CBOR.encode(100), [0x18, 0x64])
        XCTAssertEqual("hello".encode(), [0x65, 0x68, 0x65, 0x6c, 0x6c, 0x6f])
        XCTAssertEqual(CBOR.encode(["a", "b", "c"]), [0x83, 0x61, 0x61, 0x61, 0x62, 0x61, 0x63])

        struct MyStruct: CBOREncodable {
            var x: Int
            var y: String

            public func encode() -> [UInt8] {
                let cborWrapper : CBOR = [
                    "x": CBOR(integerLiteral: self.x),
                    "y": .utf8String(self.y)
                ]
                return cborWrapper.encode()
            }
        }

        let encoded = MyStruct(x: 42, y: "words").encode()
        XCTAssert(
            encoded == [0xa2, 0x61, 0x79, 0x65, 0x77, 0x6f, 0x72, 0x64, 0x73, 0x61, 0x78, 0x18, 0x2a]
            || encoded == [0xa2, 0x61, 0x78, 0x18, 0x2a, 0x61, 0x79, 0x65, 0x77, 0x6f, 0x72, 0x64, 0x73]
        )
    }
}

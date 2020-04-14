import XCTest
import SwiftCBOR
@testable import FbSmo

class CborTests: XCTestCase {
    func testEmptyData() {
        XCTAssertThrowsError(try FbSmo(data: Data(), using: .cbor)) { error in
            XCTAssertEqual(error as NSError, CBORError.unfinishedSequence as NSError)
        }
    }

    func testCorruptFirstByte() {
        XCTAssertThrowsError(try FbSmo(data: Data([0xfe]), using: .cbor)) { error in
            XCTAssertEqual(error as NSError, FbSmoCborError.dataCorrupted as NSError)
        }
    }

    func testUnsupportedValue() {
        func test(_ cbor: CBOR, issue: CBOR? = nil) {
            XCTAssertThrowsError(try FbSmo(cbor: cbor)) { error in
                XCTAssertEqual(error as NSError, FbSmoCborError.unsupportedValue(issue ?? cbor) as NSError)
            }
        }

        test(.break)
        test(.tagged(CBOR.Tag(rawValue: 0), .null))
        test(.simple(0))
        test(.half(0))
    }

    func testUnsupportedObjectKey() {
        func test(_ cbor: CBOR, issue: CBOR? = nil) {
            XCTAssertThrowsError(try FbSmo(cbor: cbor)) { error in
                XCTAssertEqual(error as NSError, FbSmoCborError.unsupportedObjectKey(issue ?? cbor) as NSError)
            }
        }

        test([.null: 0], issue: .null)
    }

    func testUnsupportedRange() {
        func test(_ cbor: CBOR, issue: CBOR? = nil) {
            XCTAssertThrowsError(try FbSmo(cbor: cbor)) { error in
                XCTAssertEqual(error as NSError, FbSmoCborError.unsupportedRange(issue ?? cbor) as NSError)
            }
        }

        test(.unsignedInt(UInt64.max))
        test(.negativeInt(UInt64.max))

        test([.unsignedInt(UInt64.max)], issue: .unsignedInt(UInt64.max))
        test([.negativeInt(UInt64.max)], issue: .negativeInt(UInt64.max))
    }
}

import XCTest
@testable import SwiftCBORTests

XCTMain([
    testCase(CBORDecoderTests.allTests),
    testCase(CBOREncoderTests.allTests),
    testCase(CodableCBOREncoderTests.allTests),
    testCase(CodableCBORDecoderTests.allTests),
    testCase(CBORCodableRoundtripTests.allTests),
    testCase(CBORTests.allTests),
])

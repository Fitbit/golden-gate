import XCTest

extension CborTests {
    static let __allTests = [
        ("testCorruptFirstByte", testCorruptFirstByte),
        ("testEmptyData", testEmptyData),
        ("testUnsupportedObjectKey", testUnsupportedObjectKey),
        ("testUnsupportedRange", testUnsupportedRange),
        ("testUnsupportedValue", testUnsupportedValue),
    ]
}

extension ConversionTests {
    static let __allTests = [
        ("testToDouble", testToDouble),
        ("testToFloat", testToFloat),
    ]
}

extension CustomDecodingTests {
    static let __allTests = [
        ("testDecodeKeyed", testDecodeKeyed),
        ("testDecodeUnkeyed", testDecodeUnkeyed),
    ]
}

extension FbSmoCodingKeyTests {
    static let __allTests = [
        ("testCopyIntValue", testCopyIntValue),
        ("testCopyStringValue", testCopyStringValue),
        ("testEquals", testEquals),
        ("testHashValue", testHashValue),
    ]
}

extension FbSmoCompatibleFloatingPointTests {
    static let __allTests = [
        ("testDoubleFinite", testDoubleFinite),
        ("testDoubleNaN", testDoubleNaN),
        ("testDoubleNegativeInfinity", testDoubleNegativeInfinity),
        ("testDoublePositiveInfinity", testDoublePositiveInfinity),
        ("testDoubleSignalingNaN", testDoubleSignalingNaN),
        ("testFloatFinite", testFloatFinite),
        ("testFloatNaN", testFloatNaN),
        ("testFloatNegativeInfinity", testFloatNegativeInfinity),
        ("testFloatPositiveInfinity", testFloatPositiveInfinity),
        ("testFloatSignalingNaN", testFloatSignalingNaN),
    ]
}

extension FbSmoTests {
    static let __allTests = [
        ("testEquals", testEquals),
        ("testExpressible", testExpressible),
        ("testNotEquals", testNotEquals),
    ]
}

extension GraphTests {
    static let __allTests = [
        ("testEncodeDecode", testEncodeDecode),
    ]
}

extension IllegalBehaviorTests {
    static let __allTests = [
        ("testNotEncoding", testNotEncoding),
    ]
}

extension KeyStrategyTests {
    static let __allTests = [
        ("testEncodeDecodeSnakeCase", testEncodeDecodeSnakeCase),
        ("testEncodeDecodeSnakeCaseCustom", testEncodeDecodeSnakeCaseCustom),
    ]
}

extension MiscTests {
    static let __allTests = [
        ("testCbor", testCbor),
        ("testEncodeDecode", testEncodeDecode),
        ("testJson", testJson),
        ("testJsonReference", testJsonReference),
    ]
}

extension MissingKeyTests {
    static let __allTests = [
        ("testMissingKey", testMissingKey),
    ]
}

extension NestedArrays2Tests {
    static let __allTests = [
        ("testCbor", testCbor),
        ("testEncodeDecode", testEncodeDecode),
        ("testJson", testJson),
    ]
}

extension NestedArraysTests {
    static let __allTests = [
        ("testCbor", testCbor),
        ("testEncodeDecode", testEncodeDecode),
        ("testJson", testJson),
        ("testJsonReference", testJsonReference),
    ]
}

extension PrimitiveTests {
    static let __allTests = [
        ("testArray", testArray),
        ("testBoolean", testBoolean),
        ("testData", testData),
        ("testDouble", testDouble),
        ("testFloat", testFloat),
        ("testInteger16", testInteger16),
        ("testInteger32", testInteger32),
        ("testInteger64", testInteger64),
        ("testInteger8", testInteger8),
        ("testNil", testNil),
        ("testPlatformInteger", testPlatformInteger),
        ("testString", testString),
        ("testUndefined", testUndefined),
    ]
}

extension SuperTests {
    static let __allTests = [
        ("testCbor", testCbor),
        ("testJson", testJson),
    ]
}

extension TypeMismatchTests {
    static let __allTests = [
        ("testNestedTypeMismatch", testNestedTypeMismatch),
        ("testTypeMismatch", testTypeMismatch),
    ]
}

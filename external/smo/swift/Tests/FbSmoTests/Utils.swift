// Copyright 2016-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

import Foundation
import XCTest

@testable import FbSmo

/// Load a test fixture from the `data` folder.
func loadFixture(_ name: String) -> Data {
    let baseUrl = URL(fileURLWithPath: #file)
    let url = URL(string: "../../../data/tests/\(name)", relativeTo: baseUrl)!
    return try! Data(contentsOf: url)
}

/// Encode and decode a value via FbSmo.
func encodeDecode<T: Codable>(_ value: T) throws -> T {
    let encoded = try FbSmoEncoder().encode(value)
    return try FbSmoDecoder().decode(T.self, from: encoded)
}

/// Encode and decode a value via the provided format.
func encodeDecode<T: Codable>(_ value: T, using format: SerializationFormat) throws -> T {
    let encoded = try FbSmoEncoder().encode(value, using: format)
    return try FbSmoDecoder().decode(T.self, from: encoded, using: format)
}

/// Encode and decode a value using all available mechanisms and
/// verify that the resulting object equals to original object.
func encodeDecodeTest<T: Codable & Equatable>(_ value: T) {
    do { // Staying in SMO
        let processed = try! encodeDecode(value)
        XCTAssertEqual(processed, value)
    }

    do { // Going through CBOR
        let processed = try! encodeDecode(value, using: .cbor)
        XCTAssertEqual(processed, value)
    }

    do { // Going through JSON
        let processed = try encodeDecode(value, using: .json)
        XCTAssertEqual(processed, value)
    } catch let error {
        let nsError = error as NSError
        let expectedNsError = FbSmoJsonError.invalidTopLevelElement(3) as NSError
        XCTAssertEqual(nsError, expectedNsError)
    }
}

func encodeDecodeTest(_ value: FbSmo) {
    do { // Going through CBOR
        let processed = try! FbSmo(data: value.data(using: .cbor), using: .cbor)
        XCTAssertEqual(processed, value)
    }
}

extension Error {
    var rawDebugDescription: String? {
        return (self as NSError).userInfo[NSDebugDescriptionErrorKey] as? String
    }
}

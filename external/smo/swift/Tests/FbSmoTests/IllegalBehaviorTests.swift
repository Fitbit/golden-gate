// Copyright 2016-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

import XCTest
@testable import FbSmo

class IllegalBehaviorTests: XCTestCase {
    func testNotEncoding() throws {
        XCTAssertThrowsError(try FbSmoEncoder().encode(NotEncoding())) { error in
            XCTAssertEqual(error.rawDebugDescription, "Top-level NotEncoding did not encode any values.")
        }
    }
}

struct NotEncoding: Encodable {
    func encode(to encoder: Encoder) throws {
        // nothing
    }
}

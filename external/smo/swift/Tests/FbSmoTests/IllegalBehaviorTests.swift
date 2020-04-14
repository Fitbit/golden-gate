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

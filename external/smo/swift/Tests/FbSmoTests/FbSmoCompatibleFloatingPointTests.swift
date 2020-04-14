import XCTest
@testable import FbSmo

class FbSmoCompatibleFloatingPointTests: XCTestCase {
    // MARK: - Float

    func testFloatFinite() {
        XCTAssertEqual(Float(exactlyKeepingInfinite: 0), 0)
        XCTAssertEqual(Float(exactlyKeepingInfinite: 1), 1)
        XCTAssertEqual(Float(exactlyKeepingInfinite: -1), -1)
        XCTAssertEqual(Float(exactlyKeepingInfinite: .leastNonzeroMagnitude), nil)
    }

    func testFloatNaN() {
        let value = Float(exactlyKeepingInfinite: .nan)
        XCTAssertEqual(value?.isNaN, true)
        XCTAssertEqual(value?.isSignalingNaN, false)
    }

    func testFloatSignalingNaN() {
        let value = Float(exactlyKeepingInfinite: .signalingNaN)
        XCTAssertEqual(value?.isNaN, true)
        XCTAssertEqual(value?.isSignalingNaN, true)
    }

    func testFloatPositiveInfinity() {
        let value = Float(exactlyKeepingInfinite: .infinity)
        XCTAssertEqual(value, .infinity)
    }

    func testFloatNegativeInfinity() {
        let value = Float(exactlyKeepingInfinite: -.infinity)
        XCTAssertEqual(value, -.infinity)
    }

    // MARK: - Double

    func testDoubleFinite() {
        XCTAssertEqual(Double(exactlyKeepingInfinite: 0), 0)
        XCTAssertEqual(Double(exactlyKeepingInfinite: 1), 1)
        XCTAssertEqual(Double(exactlyKeepingInfinite: -1), -1)
        XCTAssertEqual(Double(exactlyKeepingInfinite: .leastNonzeroMagnitude), .leastNonzeroMagnitude)
    }

    func testDoubleNaN() {
        let value = Double(exactlyKeepingInfinite: .nan)
        XCTAssertEqual(value?.isNaN, true)
        XCTAssertEqual(value?.isSignalingNaN, false)
    }

    func testDoubleSignalingNaN() {
        let value = Double(exactlyKeepingInfinite: .signalingNaN)
        XCTAssertEqual(value?.isNaN, true)
        XCTAssertEqual(value?.isSignalingNaN, true)
    }

    func testDoublePositiveInfinity() {
        let value = Double(exactlyKeepingInfinite: .infinity)
        XCTAssertEqual(value, .infinity)
    }

    func testDoubleNegativeInfinity() {
        let value = Double(exactlyKeepingInfinite: -.infinity)
        XCTAssertEqual(value, -.infinity)
    }
}

import XCTest
@testable import FbSmo

class KeyStrategyTests: XCTestCase {
    func testEncodeDecodeSnakeCase() {
        let actual = Snakehouse(boring: 0, specialOffer: "Fun", isThisAwesome: true)
        let encoder = FbSmoEncoder()
        encoder.keyEncodingStrategy = .convertToSnakeCase
        let encoded = try! encoder.encode(actual)

        guard case .object(let object) = encoded else {
            XCTFail("Expected object, but got: \(encoded)")
            return
        }

        XCTAssertEqual(Set(object.keys), ["boring", "is_this_awesome", "special_offer"])

        let decoder = FbSmoDecoder()
        decoder.keyDecodingStrategy = .convertFromSnakeCase
        let decoded = try! decoder.decode(type(of: actual), from: encoded)

        XCTAssertEqual(decoded, actual)
    }

    func testEncodeDecodeSnakeCaseCustom() {
        let actual = CustomSnakehouse(boring: 0, specialOffer: "Fun", isThisAwesome: true)
        let encoder = FbSmoEncoder()
        encoder.keyEncodingStrategy = .convertToSnakeCase
        let encoded = try! encoder.encode(actual)

        guard case .object(let object) = encoded else {
            XCTFail("Expected object, but got: \(encoded)")
            return
        }

        XCTAssertEqual(Set(object.keys), ["boring", "spec_off", "is_awesome"])

        let decoder = FbSmoDecoder()
        decoder.keyDecodingStrategy = .convertFromSnakeCase
        let decoded = try! decoder.decode(type(of: actual), from: encoded)

        XCTAssertEqual(decoded, actual)
    }
}

private struct Snakehouse: Codable, Equatable {
    let boring: Int
    let specialOffer: String
    let isThisAwesome: Bool
}

private struct CustomSnakehouse: Codable, Equatable {
    let boring: Int
    let specialOffer: String
    let isThisAwesome: Bool

    enum CodingKeys: String, CodingKey {
        case boring = "boring"
        case specialOffer = "specOff"
        case isThisAwesome = "isAwesome"
    }
}

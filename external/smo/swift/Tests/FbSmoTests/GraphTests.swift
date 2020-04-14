import XCTest
@testable import FbSmo

class GraphTests: XCTestCase {
    private let expected = Graph(
        title: "My graph",
        circle: Circle(
            radius: 0x0102030405060708,
            center: Position(x: -123, y: 2)
        ),
        miscData: [1.2, 3.456, 5.678]
    )

    func testEncodeDecode() {
        encodeDecodeTest(expected)
    }
}

private struct Graph: Codable, Equatable {
    let title: String
    let circle: Circle
    let miscData: [Float]

    enum CodingKeys: String, CodingKey {
        case title
        case circle
        case miscData = "miscellaneous_data"
    }
}

private struct Circle: Codable, Equatable {
    let radius: UInt64
    let center: Position
}

private struct Position: Codable, Equatable {
    let x: Int8
    let y: Int8
}

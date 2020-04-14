import struct Foundation.Data
import class Foundation.NSData

extension _FbSmoDecoder {
    /// A `SingleValueDecodingContainer` for SMO.
    struct SingleValueContainer {
        private let decoder: _FbSmoDecoder
        private let storage: FbSmo
        let codingPath: [CodingKey]

        /// Creates a new decoder.
        init(referencing decoder: _FbSmoDecoder, storage: FbSmo, codingPath: [CodingKey]) {
            self.decoder = decoder
            self.storage = storage
            self.codingPath = codingPath
        }
    }
}

private extension _FbSmoDecoder.SingleValueContainer {
    /// Try to decode a binary integer, casting if necessary.
    func decodeBinaryInteger<T: BinaryInteger>(_ type: T.Type) throws -> T {
        switch self.storage {
        case .float(let float):
            guard let value = T(exactly: float) else {
                throw DecodingError._typeMismatch(
                    at: self.codingPath,
                    expectation: type,
                    reality: self.storage
                )
            }
            return value
        case .integer(let integer):
            guard let value = T(exactly: integer) else {
                throw DecodingError._typeMismatch(
                    at: self.codingPath,
                    expectation: type,
                    reality: self.storage
                )
            }
            return value
        default:
            throw DecodingError._typeMismatch(
                at: self.codingPath,
                expectation: type,
                reality: self.storage
            )
        }
    }

    /// Try to decode a floating-point number, casting if necessary.
    func decodeFloatingPoint<T: FbSmoCompatibleFloatingPoint>(_ type: T.Type) throws -> T {
        switch self.storage {
        case .float(let float):
            guard let value = T.from(exactlyKeepingInfinite: float) else {
                throw DecodingError._typeMismatch(
                    at: self.codingPath,
                    expectation: type,
                    reality: self.storage
                )
            }
            return value
        case .integer(let integer):
            guard let value = T(exactly: integer) else {
                throw DecodingError._typeMismatch(
                    at: self.codingPath,
                    expectation: type,
                    reality: self.storage
                )
            }
            return value
        default:
            throw DecodingError._typeMismatch(
                at: self.codingPath,
                expectation: type,
                reality: self.storage
            )
        }
    }
}

/// Custom decoding strategies
extension _FbSmoDecoder.SingleValueContainer {
    func decode(_ type: Data.Type) throws -> Data {
        guard case .bytes(let value) = self.storage else {
            throw DecodingError._typeMismatch(
                at: self.codingPath,
                expectation: type,
                reality: self.storage
            )
        }

        return value
    }
}

extension _FbSmoDecoder.SingleValueContainer: SingleValueDecodingContainer {
    func decodeNil() -> Bool {
        switch self.storage {
        case .symbol(.null):
            return true
        default:
            return false
        }
    }

    func decode(_ type: Bool.Type) throws -> Bool {
        switch self.storage {
        case .symbol(.true):
            return true
        case .symbol(.false):
            return false
        default:
            throw DecodingError._typeMismatch(
                at: self.codingPath,
                expectation: type,
                reality: self.storage
            )
        }
    }

    func decode(_ type: String.Type) throws -> String {
        guard case .string(let value) = self.storage else {
            throw DecodingError._typeMismatch(
                at: self.codingPath,
                expectation: type,
                reality: self.storage
            )
        }

        return value
    }

    func decode(_ type: Double.Type) throws -> Double {
        return try decodeFloatingPoint(type)
    }

    func decode(_ type: Float.Type) throws -> Float {
        return try decodeFloatingPoint(type)
    }

    func decode(_ type: Int.Type) throws -> Int {
        return try decodeBinaryInteger(type)
    }

    func decode(_ type: Int8.Type) throws -> Int8 {
        return try decodeBinaryInteger(type)
    }

    func decode(_ type: Int16.Type) throws -> Int16 {
        return try decodeBinaryInteger(type)
    }

    func decode(_ type: Int32.Type) throws -> Int32 {
        return try decodeBinaryInteger(type)
    }

    func decode(_ type: Int64.Type) throws -> Int64 {
        return try decodeBinaryInteger(type)
    }

    func decode(_ type: UInt.Type) throws -> UInt {
        return try decodeBinaryInteger(type)
    }

    func decode(_ type: UInt8.Type) throws -> UInt8 {
        return try decodeBinaryInteger(type)
    }

    func decode(_ type: UInt16.Type) throws -> UInt16 {
        return try decodeBinaryInteger(type)
    }

    func decode(_ type: UInt32.Type) throws -> UInt32 {
        return try decodeBinaryInteger(type)
    }

    func decode(_ type: UInt64.Type) throws -> UInt64 {
        return try decodeBinaryInteger(type)
    }

    func decode<T: Decodable>(_ type: T.Type) throws -> T {
        if type == Data.self || type == NSData.self {
            // Respect Data decoding strategy
            return try decode(Data.self) as! T
        } else {
            return try T(from: _FbSmoDecoder(
                storage: self.storage,
                codingPath: self.codingPath,
                options: self.decoder.options
            ))
        }
    }
}

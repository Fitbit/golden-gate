import struct Foundation.Data
import SwiftCBOR

enum FbSmoCborError: Error {
    /// An indication that CBOR parsing couldn't even start.
    case dataCorrupted

    /// An indication that a CBOR object contained values
    /// that are not supported by SMO.
    case unsupportedValue(CBOR)

    /// An indication that a CBOR object contained a map
    /// with keys that were not of type `String`.
    case unsupportedObjectKey(CBOR)

    /// An indication that a CBOR object contained a value
    /// that can't be expressed in SMO without data loss.
    case unsupportedRange(CBOR)
}

extension FbSmo {
    /// Convert from CBOR (SwiftCBOR).
    init(cbor: CBOR) throws { // swiftlint:disable:this cyclomatic_complexity
        switch cbor {
        case .unsignedInt(let unsignedInt):
            guard unsignedInt <= Int64.max else {
                throw FbSmoCborError.unsupportedRange(cbor)
            }
            self = .integer(Int64(unsignedInt))
        case .negativeInt(let negativeInt):
            let value = ~Int64(bitPattern: negativeInt)
            guard value < 0 else {
                throw FbSmoCborError.unsupportedRange(cbor)
            }
            self = .integer(value)
        case .byteString(let byteString):
            self = .bytes(Data(byteString))
        case .utf8String(let string):
            self = .string(string)
        case .array(let array):
            self = .array(try array.map { try FbSmo(cbor: $0) })
        case .map(let map):
            self = .object(Dictionary(try map.map { tuple in
                guard case .utf8String(let key) = tuple.key else {
                    throw FbSmoCborError.unsupportedObjectKey(tuple.key)
                }
                return (key, try FbSmo(cbor: tuple.value))
            }, uniquingKeysWith: { first, _ in first }))
        case .boolean(let boolean):
            self = .symbol(boolean ? .true : .false)
        case .null:
            self = .symbol(.null)
        case .undefined:
            self = .symbol(.undefined)
        case .float(let float):
            self = .float(Double(float))
        case .double(let double):
            self = .float(double)
        case .tagged, .simple, .half, .break, .date:
            throw FbSmoCborError.unsupportedValue(cbor)
        }
    }

    /// Convert to CBOR (SwiftCBOR).
    func asCBOR() -> CBOR { // swiftlint:disable:this cyclomatic_complexity
        switch self {
        case .array(let array):
            return .array(array.map { $0.asCBOR() })
        case .object(let object):
            return .map(Dictionary(object.map { tuple in
                return (CBOR.utf8String(tuple.key), tuple.value.asCBOR())
            }, uniquingKeysWith: { first, _ in first }))
        case .string(let string):
            return .utf8String(string)
        case .bytes(let bytes):
            return .byteString(Array(bytes))
        case .integer(let integer):
            if integer < 0 {
                return CBOR.negativeInt(~UInt64(bitPattern: integer))
            } else {
                return CBOR.unsignedInt(UInt64(integer))
            }
        case .float(let float):
            return .double(float)
        case .symbol(.true):
            return .boolean(true)
        case .symbol(.false):
            return .boolean(false)
        case .symbol(.null):
            return .null
        case .symbol(.undefined):
            return .undefined
        }
    }
}

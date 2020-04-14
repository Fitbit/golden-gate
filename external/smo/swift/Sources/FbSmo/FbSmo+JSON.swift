import struct Foundation.Data
import class Foundation.NSNull
import Foundation

enum FbSmoJsonError: Error {
    case invalidTopLevelElement(Any)
    case unsupportedSmoValue(FbSmo)
    case unsupportedJsonValue(Any)
}

extension FbSmo {
    /// Convert from JSON.
    init(jsonObject: Any) throws { // swiftlint:disable:this cyclomatic_complexity
        switch jsonObject {
        case let string as String:
            self = .string(string)
        case let bytes as Data:
            self = .bytes(bytes)
        case let number as NSNumber:
            switch CFNumberGetType(number) {
            case .charType:
                self = .symbol(number.boolValue ? .true : .false)
            case .sInt8Type, .sInt16Type, .sInt32Type, .sInt64Type, .shortType, .intType, .longType, .longLongType, .cfIndexType, .nsIntegerType:
                self = .integer(number.int64Value)
            case .float32Type, .float64Type, .floatType, .doubleType, .cgFloatType:
                self = .float(number.doubleValue)
            @unknown default:
                throw FbSmoJsonError.unsupportedJsonValue(jsonObject)
            }
        case let array as [AnyObject]:
            self = try .array(array.map(FbSmo.init(jsonObject:)))
        case let dictionary as [String: Any]:
            self = .object(Dictionary(try dictionary.map { tuple in
                return (tuple.key, try FbSmo(jsonObject: tuple.value))
            }, uniquingKeysWith: { first, _ in first }))
        case is NSNull:
            self = .symbol(.null)
        default:
            throw FbSmoJsonError.unsupportedJsonValue(jsonObject)
        }
    }

    /// Convert to JSON.
    func asJSONObject() throws -> Any { // swiftlint:disable:this cyclomatic_complexity
        switch self {
        case .array(let array):
            return try array.map { try $0.asJSONObject() }
        case .object(let object):
            return try Dictionary(object.map { tuple in
                return try (tuple.key, tuple.value.asJSONObject())
            }, uniquingKeysWith: { first, _ in first })
        case .string(let string):
            return string
        case .bytes(let bytes):
            return bytes
        case .integer(let integer):
            return integer as NSNumber
        case .float(let float):
            return float as NSNumber
        case .symbol(let symbol):
            switch symbol {
            case .true:
                return true
            case .false:
                return false
            case .null:
                return NSNull()
            case .undefined:
                throw FbSmoJsonError.unsupportedSmoValue(self)
            }
        }
    }
}

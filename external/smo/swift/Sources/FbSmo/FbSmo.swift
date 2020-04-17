// Copyright 2016-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

import struct Foundation.Data

public enum FbSmo {
    case array([FbSmo])
    case object([String: FbSmo])
    case string(String)
    case bytes(Data)
    case integer(Int64)
    case float(Double)
    case symbol(FbSmoSymbol)
}

public enum FbSmoSymbol {
    case `true`
    case `false`
    case null
    case undefined
}

extension FbSmo: Equatable {
    public static func == (lhs: FbSmo, rhs: FbSmo) -> Bool {
        switch (lhs, rhs) {
        case let (.array(lhsArray), .array(rhsArray)):
            return lhsArray == rhsArray
        case let (.object(lhsObject), .object(rhsObject)):
            return lhsObject == rhsObject
        case let (.string(lhsString), .string(rhsString)):
            return lhsString == rhsString
        case let (.bytes(lhsBytes), .bytes(rhsBytes)):
            return lhsBytes == rhsBytes
        case let (.integer(lhsInteger), .integer(rhsInteger)):
            return lhsInteger == rhsInteger
        case let (.float(lhsFloat), .float(rhsFloat)):
            return lhsFloat == rhsFloat
        case (.symbol(.true), .symbol(.true)):
            return true
        case (.symbol(.false), .symbol(.false)):
            return true
        case (.symbol(.null), .symbol(.null)):
            return true
        case (.symbol(.undefined), .symbol(.undefined)):
            return true
        case (.array, _):
            return false
        case (.object, _):
            return false
        case (.string, _):
            return false
        case (.bytes, _):
            return false
        case (.integer, _):
            return false
        case (.float, _):
            return false
        case (.symbol, _):
            return false
        }
    }
}

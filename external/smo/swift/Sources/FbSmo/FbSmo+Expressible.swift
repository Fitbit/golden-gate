// Copyright 2016-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

extension FbSmo: ExpressibleByNilLiteral {
    public init(nilLiteral: ()) {
        self = .symbol(nil)
    }
}

extension FbSmo: ExpressibleByIntegerLiteral {
    public init(integerLiteral value: Int) {
        self = .integer(Int64(value))
    }
}

extension FbSmo: ExpressibleByExtendedGraphemeClusterLiteral {
    public init(extendedGraphemeClusterLiteral value: String) {
        self = .string(value)
    }
}

extension FbSmo: ExpressibleByStringLiteral {
    public init(unicodeScalarLiteral value: String) {
        self = .string(value)
    }

    public init(stringLiteral value: String) {
        self = .string(value)
    }
}

extension FbSmo: ExpressibleByArrayLiteral {
    public init(arrayLiteral elements: FbSmo...) {
        self = .array(elements)
    }
}

extension FbSmo: ExpressibleByDictionaryLiteral {
    public init(dictionaryLiteral elements: (String, FbSmo)...) {
        self = .object(Dictionary(elements, uniquingKeysWith: { first, _ in first }))
    }
}

extension FbSmo: ExpressibleByBooleanLiteral {
    public init(booleanLiteral value: Bool) {
        self = .symbol(value ? .true : .false)
    }
}

extension FbSmo: ExpressibleByFloatLiteral {
    public init(floatLiteral value: Float32) {
        self = .float(Double(value))
    }
}

extension FbSmoSymbol: ExpressibleByNilLiteral {
    public init(nilLiteral: ()) {
        self = .null
    }
}

extension FbSmoSymbol: ExpressibleByBooleanLiteral {
    public init(booleanLiteral value: Bool) {
        self = value ? .true : .false
    }
}

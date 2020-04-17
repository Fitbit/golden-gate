// Copyright 2016-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

/// An implementation of CodingKey used throughout FbSmo.
struct FbSmoCodingKey: CodingKey, Equatable {
    let stringValue: String
    let intValue: Int?

    /// - SeeAlso: `CodingKey.init(stringValue:)`
    init(stringValue: String) {
        self.stringValue = stringValue
        self.intValue = nil
    }

    /// - SeeAlso: `CodingKey.init(intValue:)`
    init(intValue: Int) {
        self.stringValue = "\(intValue)"
        self.intValue = intValue
    }

    /// Create a coding key from another coding key.
    init<Key>(_ base: Key) where Key: CodingKey {
        if let intValue = base.intValue {
            self.init(intValue: intValue)
        } else {
            self.init(stringValue: base.stringValue)
        }
    }

    /// Special key that will hold data for the super class.
    /// See `_JSONKey.super` in the Swift stdlib for more info.
    static let `super` = FbSmoCodingKey(stringValue: "super")
}

extension FbSmoCodingKey: Hashable {
    func hash(into hasher: inout Hasher) {
        hasher.combine(intValue)
        hasher.combine(stringValue)
    }
}

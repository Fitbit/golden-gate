// Copyright 2016-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

/// Used to avoid creating a hierarchy of encoding/decoding containers.
///
/// A nested container will not reference it's parent container
/// but a `Storage` instead.
protocol Storage {
    func tryUnwrap() -> FbSmo?
}

/// A storage that can be passed to other containers
/// to write their results into.
class IndirectStorage: Storage {
    var value: Storage? {
        willSet {
            precondition(value == nil, "Storage already occupied by: \(String(describing: value))")
        }
    }

    func tryUnwrap() -> FbSmo? {
        return value?.tryUnwrap()
    }
}

/// A storage used by single value containers.
class SingleValueStorage: Storage {
    var value: FbSmo? {
        willSet {
            precondition(value == nil, "Attempt to encode value through single value container when previously value already encoded.")
        }
    }

    func tryUnwrap() -> FbSmo? {
        return value
    }
}

// MARK: - Direct Storage Types

/// Storage that is not indirect.
protocol DirectStorage: Storage {
    func unwrap() -> FbSmo
}

/// Default implementation for `Storage` conformance
extension DirectStorage {
    func tryUnwrap() -> FbSmo? {
        return unwrap()
    }
}

/// A storage used by keyed containers.
class KeyedStorage: DirectStorage {
    var value = [String: Storage]()

    func unwrap() -> FbSmo {
        return .object(value.mapValues { $0.tryUnwrap() ?? .symbol(.undefined) })
    }
}

/// A storage used by unkeyed containers.
class UnkeyedStorage: DirectStorage {
    var value = [Storage]()

    func unwrap() -> FbSmo {
        return .array(value.map { $0.tryUnwrap() ?? .symbol(.undefined) })
    }
}

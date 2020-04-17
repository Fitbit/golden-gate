// Copyright 2016-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

import struct Foundation.Data

extension _FbSmoDecoder {
    struct KeyedContainer<Key> where Key: CodingKey {
        private let decoder: _FbSmoDecoder
        private let storage: [String: FbSmo]
        let codingPath: [CodingKey]

        init(referencing decoder: _FbSmoDecoder, storage: [String: FbSmo], codingPath: [CodingKey]) {
            self.decoder = decoder

            // Apply key decoding strategy
            switch decoder.options.keyDecodingStrategy {
            case .useDefaultKeys:
                self.storage = storage
            case .convertFromSnakeCase:
                self.storage = Dictionary(storage.map { tuple in
                    (FbSmoDecoder.KeyDecodingStrategy._convertFromSnakeCase(tuple.key), tuple.value)
                }, uniquingKeysWith: { first, _ in first })
            case .custom(let converter):
                self.storage = Dictionary(storage.map { tuple in
                    (converter(decoder.codingPath + [FbSmoCodingKey(stringValue: tuple.key)]).stringValue, tuple.value)
                }, uniquingKeysWith: { first, _ in first })
            }

            self.codingPath = codingPath
        }

        init(referencing decoder: _FbSmoDecoder, storage: FbSmo, codingPath: [CodingKey]) throws {
            guard case .object(let object) = storage else {
                throw DecodingError._typeMismatch(
                    at: codingPath,
                    expectation: [String: Any].self,
                    reality: storage
                )
            }

            self.init(
                referencing: decoder,
                storage: object,
                codingPath: codingPath
            )
        }
    }
}

private extension _FbSmoDecoder.KeyedContainer {
    /// Get a new coding path by appending the provided key
    /// to the current coding path.
    func nestedCodingPath(forKey key: CodingKey) -> [CodingKey] {
        return self.codingPath + [key]
    }

    /// Utility to create a decoder for calling `super.init(from:)`.
    func makeSuperDecoder(forKey key: CodingKey) -> Decoder {
        let storage = self.storage[key.stringValue] ?? .symbol(.undefined)

        return _FbSmoDecoder(
            storage: storage,
            codingPath: self.nestedCodingPath(forKey: key),
            options: self.decoder.options
        )
    }

    // https://github.com/apple/swift/blob/2b04e9f1058db3102ca5d7cdcb5fd18ecd37b4b7/stdlib/public/SDK/Foundation/JSONEncoder.swift
    func _errorDescription(of key: CodingKey) -> String {
        switch decoder.options.keyDecodingStrategy {
        case .convertFromSnakeCase:
            // In this case we can attempt to recover the original value by reversing the transform
            let original = key.stringValue
            let converted = FbSmoEncoder.KeyEncodingStrategy._convertToSnakeCase(original)
            if converted == original {
                return "\(key) (\"\(original)\")"
            } else {
                return "\(key) (\"\(original)\"), converted to \(converted)"
            }
        case .useDefaultKeys, .custom:
            // Otherwise, just report the converted string
            return "\(key) (\"\(key.stringValue)\")"
        }
    }

    /// Gets the value for the given key or throws a `DecodingError.keyNotFound` error.
    func decodeValue(forKey key: Key) throws -> FbSmo {
        guard let value = self.storage[key.stringValue] else {
            throw DecodingError.keyNotFound(key, DecodingError.Context(
                codingPath: self.codingPath,
                debugDescription: "No value associated with key \(_errorDescription(of: key))."
            ))
        }

        return value
    }
}

extension _FbSmoDecoder.KeyedContainer: KeyedDecodingContainerProtocol {
    var allKeys: [Key] {
        return self.storage.keys.compactMap { Key(stringValue: $0) }
    }

    func contains(_ key: Key) -> Bool {
        return self.storage.keys.contains(key.stringValue)
    }

    func decodeNil(forKey key: Key) throws -> Bool {
        let value = try decodeValue(forKey: key)
        let codingPath = nestedCodingPath(forKey: key)

        let singleValueContainer = _FbSmoDecoder.SingleValueContainer(
            referencing: self.decoder,
            storage: value,
            codingPath: codingPath
        )

        return singleValueContainer.decodeNil()
    }

    func decode<T: Decodable>(_ type: T.Type, forKey key: Key) throws -> T {
        let value = try decodeValue(forKey: key)

        let decoder = _FbSmoDecoder(
            storage: value,
            codingPath: self.nestedCodingPath(forKey: key),
            options: self.decoder.options
        )

        return try T(from: decoder)
    }

    func nestedUnkeyedContainer(forKey key: Key) throws -> UnkeyedDecodingContainer {
        return try _FbSmoDecoder.UnkeyedContainer(
            referencing: self.decoder,
            storage: try decodeValue(forKey: key),
            codingPath: nestedCodingPath(forKey: key)
        )
    }

    func nestedContainer<NestedKey>(keyedBy type: NestedKey.Type, forKey key: Key) throws -> KeyedDecodingContainer<NestedKey> where NestedKey: CodingKey {
        return try KeyedDecodingContainer(_FbSmoDecoder.KeyedContainer<NestedKey>(
            referencing: self.decoder,
            storage: try decodeValue(forKey: key),
            codingPath: self.nestedCodingPath(forKey: key)
        ))
    }

    func superDecoder() throws -> Decoder {
        return makeSuperDecoder(forKey: FbSmoCodingKey.super)
    }

    func superDecoder(forKey key: Key) throws -> Decoder {
        return makeSuperDecoder(forKey: key)
    }
}

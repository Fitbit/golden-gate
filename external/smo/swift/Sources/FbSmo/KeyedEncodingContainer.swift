import struct Foundation.Data

extension _FbSmoEncoder {
    struct KeyedContainer<Key> where Key: CodingKey {
        private let encoder: _FbSmoEncoder
        private let storage: KeyedStorage
        let codingPath: [CodingKey]

        init(referencing encoder: _FbSmoEncoder, storage: IndirectStorage, codingPath: [CodingKey]) {
            self.encoder = encoder
            self.codingPath = codingPath
            self.storage = KeyedStorage()
            storage.value = self.storage
        }
    }
}

private extension _FbSmoEncoder.KeyedContainer {
    /// Get a new coding path by appending the provided key
    /// to the current coding path.
    func nestedCodingPath(forKey key: CodingKey) -> [CodingKey] {
        return self.codingPath + [key]
    }

    /// Converts a key according to the encoder's key encoding strategy.
    func converted(_ key: CodingKey) -> CodingKey {
        switch encoder.options.keyEncodingStrategy {
        case .useDefaultKeys:
            return key
        case .convertToSnakeCase:
            let newKeyString = FbSmoEncoder.KeyEncodingStrategy._convertToSnakeCase(key.stringValue)
            return FbSmoCodingKey(stringValue: newKeyString)
        case .custom(let converter):
            return converter(codingPath + [key])
        }
    }

    /// Utility to insert a new storage into the dictionary.
    func insert<T>(_ key: CodingKey, _ closure: (IndirectStorage, [CodingKey]) -> T) -> T {
        let convertedKey = self.converted(key)

        let storage = IndirectStorage()
        self.storage.value[convertedKey.stringValue] = storage

        return closure(storage, self.nestedCodingPath(forKey: convertedKey))
    }

    /// Shared method to create a super encoder.
    func makeSuperEncoder(forKey key: CodingKey) -> Encoder {
        return insert(key) { storage, codingPath in
            return _FbSmoEncoder(
                storage: storage,
                codingPath: codingPath,
                options: self.encoder.options
            )
        }
    }

    /// Shared method to create a nested single-value container.
    func nestedSingleValueContainer(forKey key: Key) -> SingleValueEncodingContainer {
        return insert(key) { storage, codingPath in
            _FbSmoEncoder.SingleValueContainer(
                referencing: self.encoder,
                storage: storage,
                codingPath: self.nestedCodingPath(forKey: key)
            )
        }
    }
}

extension _FbSmoEncoder.KeyedContainer: KeyedEncodingContainerProtocol {
    func encodeNil(forKey key: Key) throws {
        var container = self.nestedSingleValueContainer(forKey: key)
        try container.encodeNil()
    }

    func encode<T: Encodable>(_ value: T, forKey key: Key) throws {
        var container = self.nestedSingleValueContainer(forKey: key)
        try container.encode(value)
    }

    func nestedUnkeyedContainer(forKey key: Key) -> UnkeyedEncodingContainer {
        return insert(key) { storage, codingPath in
            _FbSmoEncoder.UnkeyedContainer(
                referencing: self.encoder,
                storage: storage,
                codingPath: self.nestedCodingPath(forKey: key)
            )
        }
    }

    func nestedContainer<NestedKey>(keyedBy keyType: NestedKey.Type, forKey key: Key) -> KeyedEncodingContainer<NestedKey> where NestedKey: CodingKey {
        return insert(key) { storage, codingPath in
            KeyedEncodingContainer(_FbSmoEncoder.KeyedContainer<NestedKey>(
                referencing: self.encoder,
                storage: storage,
                codingPath: self.nestedCodingPath(forKey: key)
            ))
        }
    }

    func superEncoder() -> Encoder {
        return makeSuperEncoder(forKey: FbSmoCodingKey.super)
    }

    func superEncoder(forKey key: Key) -> Encoder {
        return makeSuperEncoder(forKey: key)
    }
}

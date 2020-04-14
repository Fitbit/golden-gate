import struct Foundation.Data

extension _FbSmoDecoder {
    struct UnkeyedContainer {
        private let decoder: _FbSmoDecoder
        private let storage: [FbSmo]
        internal let codingPath: [CodingKey]

        fileprivate(set) var currentIndex: Int = 0

        init(referencing decoder: _FbSmoDecoder, storage: [FbSmo], codingPath: [CodingKey]) {
            self.decoder = decoder
            self.storage = storage
            self.codingPath = codingPath
        }

        init(referencing decoder: _FbSmoDecoder, storage: FbSmo, codingPath: [CodingKey]) throws {
            switch storage {
            case .array(let array):
                self.init(
                    referencing: decoder,
                    storage: array,
                    codingPath: codingPath
                )
            case .bytes(let data):
                self.init(
                    referencing: decoder,
                    storage: Array(data).map { .integer(Int64($0)) },
                    codingPath: codingPath
                )
            default:
                throw DecodingError._typeMismatch(
                    at: codingPath,
                    expectation: [Any].self,
                    reality: storage
                )
            }
        }
    }
}

private extension _FbSmoDecoder.UnkeyedContainer {
    /// Get a new coding path by appending the provided key
    /// to the current coding path.
    func nestedCodingPath(forIndex index: Int) -> [CodingKey] {
        return self.codingPath + [FbSmoCodingKey(intValue: index)]
    }

    /// Gets the element at the given index or throws a
    /// `DecodingError.dataCorruptedError` error on OOB access.
    func decodeValue() throws -> FbSmo {
        guard !self.isAtEnd else {
            throw DecodingError.dataCorruptedError(in: self, debugDescription: "Unexpected end of data")
        }

        return self.storage[self.currentIndex]
    }
}

extension _FbSmoDecoder.UnkeyedContainer: UnkeyedDecodingContainer {
    var count: Int? {
        return self.storage.count
    }

    var isAtEnd: Bool {
        return self.currentIndex >= self.storage.count
    }

    mutating func decodeNil() throws -> Bool {
        let value = try self.decodeValue()
        // no defer { ... } !

        let codingPath = self.nestedCodingPath(forIndex: self.currentIndex)

        let singleValueContainer = _FbSmoDecoder.SingleValueContainer(
            referencing: self.decoder,
            storage: value,
            codingPath: codingPath
        )

        // From the protocol documentation of `UnkeyedDecodingContainer.decodeNil()`:
        // > If the value is not null, does not increment currentIndex.
        guard singleValueContainer.decodeNil() else {
            return false
        }

        self.currentIndex += 1
        return true
    }

    mutating func decode<T: Decodable>(_ type: T.Type) throws -> T {
        let value = try self.decodeValue()
        defer { self.currentIndex += 1 }

        return try T(from: _FbSmoDecoder(
            storage: value,
            codingPath: self.codingPath,
            options: self.decoder.options
        ))
    }

    mutating func nestedUnkeyedContainer() throws -> UnkeyedDecodingContainer {
        let value = try self.decodeValue()
        defer { self.currentIndex += 1 }

        return try _FbSmoDecoder.UnkeyedContainer(
            referencing: self.decoder,
            storage: value,
            codingPath: self.nestedCodingPath(forIndex: self.currentIndex)
        )
    }

    mutating func nestedContainer<NestedKey>(keyedBy type: NestedKey.Type) throws -> KeyedDecodingContainer<NestedKey> where NestedKey: CodingKey {
        let value = try self.decodeValue()
        defer { self.currentIndex += 1 }

        return try KeyedDecodingContainer(_FbSmoDecoder.KeyedContainer<NestedKey>(
            referencing: self.decoder,
            storage: value,
            codingPath: self.nestedCodingPath(forIndex: self.currentIndex)
        ))
    }

    mutating func superDecoder() throws -> Decoder {
        let value = try decodeValue()
        defer { self.currentIndex += 1 }

        return _FbSmoDecoder(
            storage: value,
            codingPath: self.nestedCodingPath(forIndex: self.currentIndex),
            options: self.decoder.options
        )
    }
}

// Copyright 2016-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

import struct Foundation.Data

extension _FbSmoEncoder {
    struct UnkeyedContainer {
        private let encoder: _FbSmoEncoder
        private let storage: UnkeyedStorage
        let codingPath: [CodingKey]

        init(referencing encoder: _FbSmoEncoder, storage: IndirectStorage, codingPath: [CodingKey]) {
            self.encoder = encoder
            self.storage = UnkeyedStorage()
            self.codingPath = codingPath

            storage.value = self.storage
        }
    }
}

private extension _FbSmoEncoder.UnkeyedContainer {
    /// Get a new coding path by appending the provided key
    /// to the current coding path.
    func nestedCodingPath(forIndex index: Int) -> [CodingKey] {
        return self.codingPath + [FbSmoCodingKey(intValue: index)]
    }

    /// Append a new storage to the array.
    func append<T>(_ closure: (IndirectStorage, [CodingKey]) -> T) -> T {
        // Get index before we append to the storage.
        let codingPath = self.nestedCodingPath(forIndex: self.count)

        let storage = IndirectStorage()
        self.storage.value.append(storage)

        return closure(storage, codingPath)
    }

    /// Append a new nested single-value container.
    func nestedSingleValueContainer() -> SingleValueEncodingContainer {
        return self.append { storage, codingPath in
            _FbSmoEncoder.SingleValueContainer(
                referencing: self.encoder,
                storage: storage,
                codingPath: codingPath
            )
        }
    }
}

extension _FbSmoEncoder.UnkeyedContainer: UnkeyedEncodingContainer {
    var count: Int {
        return self.storage.value.count
    }

    func encodeNil() throws {
        var container = self.nestedSingleValueContainer()
        try container.encodeNil()
    }

    func encode<T: Encodable>(_ value: T) throws {
        var container = self.nestedSingleValueContainer()
        try container.encode(value)
    }

    func nestedContainer<NestedKey>(keyedBy keyType: NestedKey.Type) -> KeyedEncodingContainer<NestedKey> where NestedKey: CodingKey {
        return self.append { storage, codingPath in
            KeyedEncodingContainer(_FbSmoEncoder.KeyedContainer<NestedKey>(
                referencing: self.encoder,
                storage: storage,
                codingPath: codingPath
            ))
        }
    }

    func nestedUnkeyedContainer() -> UnkeyedEncodingContainer {
        return self.append { storage, codingPath in
            _FbSmoEncoder.UnkeyedContainer(
                referencing: self.encoder,
                storage: storage,
                codingPath: codingPath
            )
        }
    }

    func superEncoder() -> Encoder {
        return self.append { storage, codingPath in
            _FbSmoEncoder(
                storage: storage,
                codingPath: codingPath,
                options: self.encoder.options
            )
        }
    }
}

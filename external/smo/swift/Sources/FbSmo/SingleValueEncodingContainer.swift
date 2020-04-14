import struct Foundation.Data
import class Foundation.NSData

extension _FbSmoEncoder {
    /// A `SingleValueEncodingContainer` for SMO.
    struct SingleValueContainer {
        private let encoder: _FbSmoEncoder
        private let storage: SingleValueStorage
        let codingPath: [CodingKey]

        init(referencing encoder: _FbSmoEncoder, storage: IndirectStorage, codingPath: [CodingKey]) {
            self.encoder = encoder
            self.storage = SingleValueStorage()
            self.codingPath = codingPath

            storage.value = self.storage
        }
    }
}

private extension _FbSmoEncoder.SingleValueContainer {
    func encodeBinaryInteger<T: BinaryInteger>(_ value: T) throws {
        guard let integer = Int64(exactly: value) else {
            guard let float = Double(exactly: value) else {
                throw EncodingError.invalidValue(value, EncodingError.Context(
                    codingPath: self.codingPath,
                    debugDescription: "Unable to encode \(value) to SMO."
                ))
            }

            self.storage.value = .float(float)
            return
        }

        self.storage.value = .integer(Int64(integer))
    }
}

/// Custom encoding strategies
private extension _FbSmoEncoder.SingleValueContainer {
    func encode(_ value: Data) throws {
        self.storage.value = .bytes(value)
    }
}

extension _FbSmoEncoder.SingleValueContainer: SingleValueEncodingContainer {
    func encodeNil() throws {
        self.storage.value = .symbol(.null)
    }

    func encode(_ value: Bool) throws {
        self.storage.value = .symbol(value ? .true : .false)
    }

    func encode(_ value: String) throws {
        self.storage.value = .string(value)
    }

    func encode(_ value: Double) throws {
        self.storage.value = .float(value)
    }

    func encode(_ value: Float) throws {
        self.storage.value = .float(Double(value))
    }

    func encode(_ value: Int) throws {
        try self.encodeBinaryInteger(value)
    }

    func encode(_ value: Int8) throws {
        try self.encodeBinaryInteger(value)
    }

    func encode(_ value: Int16) throws {
        try self.encodeBinaryInteger(value)
    }

    func encode(_ value: Int32) throws {
        try self.encodeBinaryInteger(value)
    }

    func encode(_ value: Int64) throws {
        try self.encodeBinaryInteger(value)
    }

    func encode(_ value: UInt) throws {
        try self.encodeBinaryInteger(value)
    }

    func encode(_ value: UInt8) throws {
        try self.encodeBinaryInteger(value)
    }

    func encode(_ value: UInt16) throws {
        try self.encodeBinaryInteger(value)
    }

    func encode(_ value: UInt32) throws {
        try self.encodeBinaryInteger(value)
    }

    func encode(_ value: UInt64) throws {
        try self.encodeBinaryInteger(value)
    }

    func encode<T: Encodable>(_ value: T) throws {
        if T.self == Data.self || T.self == NSData.self {
            // Respect Data encoding strategy
            try self.encode(value as! Data)
        } else {
            let storage = IndirectStorage()
            let encoder = _FbSmoEncoder(
                storage: storage,
                codingPath: self.codingPath,
                options: self.encoder.options
            )
            try value.encode(to: encoder)
            self.storage.value = storage.tryUnwrap()
        }
    }
}

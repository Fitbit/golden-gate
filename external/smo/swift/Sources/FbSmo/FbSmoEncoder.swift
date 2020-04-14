import struct Foundation.Data
import class Foundation.NSData

/// `FbSmoEncoder` facilitates the encoding of `Encodable` to SMO formats.
open class FbSmoEncoder {
    /// The strategy to use for encoding keys. Defaults to `.useDefaultKeys`.
    open var keyEncodingStrategy: KeyEncodingStrategy = .useDefaultKeys

    /// Contextual user-provided information for use during encoding.
    open var userInfo: [CodingUserInfoKey: Any] = [:]

    /// The options set on the top-level encoder.
    private var options: _Options {
        return _Options(
            keyEncodingStrategy: keyEncodingStrategy,
            userInfo: userInfo
        )
    }

    /// Creates a new encoder.
    public init() {}

    /// Encodes the given top-level value and returns its encoded SMO representation.
    ///
    /// - Parameter value: The value to encode.
    /// - Returns: A new `FbSmo` value containing the encoded data.
    /// - Throws: An error if any value throws an error during encoding.
    public func encode<T: Encodable>(_ value: T) throws -> FbSmo {
        var encoder = _FbSmoEncoder(codingPath: [], options: self.options)
        return try encoder.encode(value)
    }

    /// Encodes the given top-level value and returns its encoded SMO representation.
    ///
    /// - Parameters:
    ///   - value: The value to encode.
    ///   - format: The serialization format to encode to.
    /// - Returns: A new `Data` value containing the encoded JSON data.
    /// - throws: An error if any value throws an error during encoding.
    public func encode<T: Encodable>(_ value: T, using format: SerializationFormat) throws -> Data {
        return try self.encode(value).data(using: format)
    }
}

struct _FbSmoEncoder {
    /// - SeeAlso: `Encoder.codingPath`
    let codingPath: [CodingKey]

    /// Options set on the top-level decoder.
    let options: FbSmoEncoder._Options

    /// The storage this encoder writes into.
    private let storage: IndirectStorage

    init(storage: IndirectStorage = IndirectStorage(), codingPath: [CodingKey], options: FbSmoEncoder._Options) {
        self.storage = storage
        self.codingPath = codingPath
        self.options = options
    }
}

private extension _FbSmoEncoder {
    mutating func encode<T: Encodable>(_ value: T) throws -> FbSmo {
        // Respect custom encoding strategies by going through `singleValueContainer`
        var container = self.singleValueContainer()
        try container.encode(value)

        guard let topLevel = storage.tryUnwrap() else {
            throw EncodingError.invalidValue(
                value,
                EncodingError.Context(
                    codingPath: [],
                    debugDescription: "Top-level \(T.self) did not encode any values."
                )
            )
        }

        return topLevel
    }
}

extension _FbSmoEncoder: Encoder {
    var userInfo: [CodingUserInfoKey: Any] {
        return self.options.userInfo
    }

    func container<Key>(keyedBy type: Key.Type) -> KeyedEncodingContainer<Key> where Key: CodingKey {
        return KeyedEncodingContainer(KeyedContainer<Key>(
            referencing: self,
            storage: self.storage,
            codingPath: self.codingPath
        ))
    }

    func unkeyedContainer() -> UnkeyedEncodingContainer {
        return UnkeyedContainer(
            referencing: self,
            storage: self.storage,
            codingPath: self.codingPath
        )
    }

    func singleValueContainer() -> SingleValueEncodingContainer {
        return SingleValueContainer(
            referencing: self,
            storage: self.storage,
            codingPath: self.codingPath
        )
    }
}

import struct Foundation.Data
import class Foundation.NSData

/// `FbSmoDecoder` facilitates the decoding of `Decodable` from SMO formats.
open class FbSmoDecoder {
    /// The strategy to use for decoding keys. Defaults to `.useDefaultKeys`.
    open var keyDecodingStrategy: KeyDecodingStrategy = .useDefaultKeys

    /// Contextual user-provided information for use during decoding.
    open var userInfo: [CodingUserInfoKey: Any] = [:]

    /// The options set on the top-level decoder.
    private var options: _Options {
        return _Options(
            keyDecodingStrategy: keyDecodingStrategy,
            userInfo: userInfo
        )
    }

    /// Creates a new decoder.
    public init() {}

    /// Decodes a top-level value of the given type from the given SMO representation.
    ///
    /// - Parameters:
    ///   - type: The type of the value to decode.
    ///   - smo: The SMO to decode from.
    /// - Returns: A value of the requested type.
    /// - Throws: `DecodingError.dataCorrupted` if values requested from the payload are corrupted, or if the given data is not valid.
    /// - Throws: An error if any value throws an error during decoding.
    public func decode<T: Decodable>(_ type: T.Type, from smo: FbSmo) throws -> T {
        let decoder = _FbSmoDecoder(
            storage: smo,
            codingPath: [],
            options: self.options
        )

        return try decoder.decode(type)
    }

    /// Decodes a top-level value of the given type from the given SMO format.
    ///
    /// - Parameters:
    ///   - type: The type of the value to decode.
    ///   - data: The data to decode from.
    ///   - format: The format the SMO was serialized to.
    /// - Returns: A value of the requested type.
    /// - Throws: `DecodingError.dataCorrupted` if values requested from the payload are corrupted, or if the given data is not valid.
    /// - Throws: An error if any value throws an error during decoding.
    public func decode<T: Decodable>(_ type: T.Type, from data: Data, using format: SerializationFormat) throws -> T {
        let smo = try FbSmo(data: data, using: format)
        return try self.decode(type, from: smo)
    }
}

struct _FbSmoDecoder {
    /// The decoder's storage.
    private let storage: FbSmo

    /// The path to the current point in decoding.
    let codingPath: [CodingKey]

    /// Options set on the top-level decoder.
    let options: FbSmoDecoder._Options

    /// Contextual user-provided information for use during encoding.
    public var userInfo: [CodingUserInfoKey: Any] {
        return self.options.userInfo
    }

    /// Create a new instance.
    ///
    /// - Parameters:
    ///   - storage: The decoder's storage.
    ///   - codingPath: The path to the current point in decoding.
    ///   - options: Options set on the top-level decoder.
    init(storage: FbSmo, codingPath: [CodingKey], options: FbSmoDecoder._Options) {
        self.storage = storage
        self.codingPath = codingPath
        self.options = options
    }
}

private extension _FbSmoDecoder {
    func decode<T: Decodable>(_ type: T.Type) throws -> T {
        // Respect custom decoding strategies by going through `singleValueContainer`
        return try self.singleValueContainer().decode(type)
    }
}

extension _FbSmoDecoder: Decoder {
    func container<Key>(keyedBy type: Key.Type) throws -> KeyedDecodingContainer<Key> where Key: CodingKey {
        return try KeyedDecodingContainer(KeyedContainer<Key>(
            referencing: self,
            storage: self.storage,
            codingPath: self.codingPath
        ))
    }

    func unkeyedContainer() throws -> UnkeyedDecodingContainer {
        return try UnkeyedContainer(
            referencing: self,
            storage: self.storage,
            codingPath: self.codingPath
        )
    }

    func singleValueContainer() throws -> SingleValueDecodingContainer {
        return SingleValueContainer(
            referencing: self,
            storage: self.storage,
            codingPath: self.codingPath
        )
    }
}

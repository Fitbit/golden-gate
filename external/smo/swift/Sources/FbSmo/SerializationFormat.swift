/// The serialization format to use.
public enum SerializationFormat {
    /// Use Concise Binary Object Representation (CBOR)
    /// https://tools.ietf.org/html/rfc7049
    case cbor

    /// Use JavaScript Object Notation (JSON)
    case json
}

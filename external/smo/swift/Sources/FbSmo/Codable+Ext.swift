// Extracted from the official Swift stdlib at
// https://github.com/apple/swift/blob/master/stdlib/public/SDK/Foundation/Codable.swift
// together with the documentation on `_typeDescription` further below.
extension DecodingError {
    /// Returns a `.typeMismatch` error describing the expected type.
    ///
    /// - parameter path: The path of `CodingKey`s taken to decode a value of this type.
    /// - parameter expectation: The type expected to be encountered.
    /// - parameter reality: The value that was encountered instead of the expected type.
    /// - returns: A `DecodingError` with the appropriate path and debug description.
    static func _typeMismatch(at path: [CodingKey], expectation: Any.Type, reality: FbSmo) -> DecodingError {
        // swiftlint:disable:previous identifier_name
        let description = "Expected to decode \(expectation) but found \(_typeDescription(of: reality)) instead."
        return .typeMismatch(expectation, Context(codingPath: path, debugDescription: description))
    }
}

private extension DecodingError {
    /// Returns a description of the type of `value` appropriate for an error message.
    ///
    /// - parameter value: The value whose type to describe.
    /// - returns: A string describing `value`.
    /// - precondition: `value` is one of the types below.
    static func _typeDescription(of value: FbSmo) -> String {
        switch value {
        case .array:
            return "an array"
        case .object:
            return "an object"
        case .string:
            return "a string"
        case .bytes:
            return "raw bytes"
        case .integer:
            return "an integer"
        case .float:
            return "a double-precision, floating-point value"
        case .symbol(.true), .symbol(.false):
            return "a boolean"
        case .symbol(.null):
            return "a null value"
        case .symbol(.undefined):
            return "an undefined value"
        }
    }
}

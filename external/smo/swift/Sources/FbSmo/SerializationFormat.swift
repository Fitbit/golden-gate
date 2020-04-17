// Copyright 2016-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

/// The serialization format to use.
public enum SerializationFormat {
    /// Use Concise Binary Object Representation (CBOR)
    /// https://tools.ietf.org/html/rfc7049
    case cbor

    /// Use JavaScript Object Notation (JSON)
    case json
}

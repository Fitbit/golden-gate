// Copyright 2016-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

import Foundation
import SwiftCBOR

extension FbSmo {
    /// Encode the object to the provided serialization format.
    public func data(using format: SerializationFormat) throws -> Data {
        switch format {
        case .cbor:
            return Data(asCBOR().encode())
        case .json:
            let jsonObject = try asJSONObject()
            guard JSONSerialization.isValidJSONObject(jsonObject) else {
                throw FbSmoJsonError.invalidTopLevelElement(jsonObject)
            }

            return try JSONSerialization.data(withJSONObject: jsonObject)
        }
    }

    /// Decode the object from the provided serialization format.
    public init(data: Data, using format: SerializationFormat) throws {
        switch format {
        case .cbor:
            guard let cbor = try CBOR.decode(Array(data)) else {
                throw FbSmoCborError.dataCorrupted
            }

            try self.init(cbor: cbor)
        case .json:
            let jsonObject = try JSONSerialization.jsonObject(with: data, options: .allowFragments)
            try self.init(jsonObject: jsonObject)
        }
    }
}

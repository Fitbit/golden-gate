//
//  CoapOutgoingBody.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 5/29/18.
//  Copyright © 2018 Fitbit. All rights reserved.
//

import Foundation

/// The body of an outgoing request or response.
internal enum CoapOutgoingBody {
    /// No body.
    case none

    /// A static buffer and an optional progress block.
    case data(Data, ((Double) -> Void)?)

    // TODO: [FC-1147] add `stream`
}

extension CoapOutgoingBody: Equatable {
    static func == (lhs: CoapOutgoingBody, rhs: CoapOutgoingBody) -> Bool {
        switch (lhs, rhs) {
        case (.none, .none): return true
        case let (.data(lhsData, _), .data(rhsData, _)) where lhsData == rhsData: return true
        default: return false
        }
    }
}

/// Utility prisms to avoid pattern matching
internal extension CoapOutgoingBody {
    var data: Data? {
        guard case .data(let data, _) = self else { return nil }
        return data
    }
}

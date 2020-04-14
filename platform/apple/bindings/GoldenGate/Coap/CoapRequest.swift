//
//  CoapRequest.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 5/29/18.
//  Copyright © 2018 Fitbit. All rights reserved.
//

import Foundation

public enum CoapRequestError: Error {
    /// The error being used when `expectsSuccess` was set to true
    /// but the server sent a non-2.xx response.
    case responseNotSuccessful(CoapCode, CoapExtendedError?)

    /// The error being used when the transport was not ready at the time
    /// the request was made.
    case transportUnavailable(reason: Error)
}

extension CoapRequestError: CustomStringConvertible {
    public var description: String {
        switch self {
        case .responseNotSuccessful(let code, let extendedError):
            return "CoapRequestError.responseNotSuccessful [\(code) \(String(describing: extendedError))]"
        case .transportUnavailable(let reason):
            return "CoapRequestError.transportUnavailable [\(reason)]"
        }
    }
}

/// A CoAP request.
public struct CoapRequest: CustomStringConvertible {
    internal let method: CoapCode.Request
    internal let options: [CoapOption]
    internal let confirmable: Bool
    internal let expectsSuccess: Bool
    internal let acceptsBlockwiseTransfer: Bool
    internal let body: CoapOutgoingBody

    /// Additional parameters used in a CoapRequest.
    /// When nil, it will use default values from XP.
    internal let parameters: GGCoapRequestParameters?

    internal var path: String {
        let parts = options
            .filter { $0.number == .uriPath }
            .compactMap { $0.value.string }
        
        return "/" + parts.joined(separator: "/")
    }

    public var description: String {
        return "\(method) \(path)"
    }
}

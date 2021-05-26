//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  CoapMethod.swift
//  GoldenGate
//
//  Created by Bogdan Vlad on 11/22/17.
//

import GoldenGateXP

/// CoAP methods conform to CoAP specification (5.8 Method Definitions).
public enum CoapMethod {
    case put
    case post
    case delete
    case get
}

/// Convert between Swift CoAP method and GG XP CoAP method.
extension CoapMethod {
    // swiftlint:disable:next identifier_name
    var gg_coapMethod: GG_CoapMethod {
        switch self {
        case .put:
            return GG_COAP_METHOD_PUT
        case .post:
            return GG_COAP_METHOD_POST
        case .get:
            return GG_COAP_METHOD_GET
        case .delete:
            return GG_COAP_METHOD_DELETE
        }
    }

    // swiftlint:disable:next identifier_name
    init(gg_coapMethod: GG_CoapMethod) {
        switch gg_coapMethod {
        case GG_COAP_METHOD_DELETE:
            self = .delete
        case GG_COAP_METHOD_GET:
            self = .get
        case GG_COAP_METHOD_POST:
            self = .post
        case GG_COAP_METHOD_PUT:
            self = .put
        default:
            preconditionFailure("Wrong coap method!")
        }
    }
}

//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  GGCoapRequestParameters.swift
//  GoldenGate
//
//  Created by Radu Vlad on 20/03/2019.
//

import GoldenGateXP

class GGCoapRequestParameters {
    typealias Ref = UnsafeMutablePointer<GG_CoapClientParameters>
    private(set) var ref: Ref

    /// Initializes additional parameters for a coap request.
    ///
    /// - Parameters:
    ///   - ackTimeout: Timeout after which a resend will happen, in milliseconds.
    ///                 Set 0 to use a default value according to the CoAP specfication and the endpoint.
    ///   - resendCount: Maximum number of times the client will resend the request
    ///                  if there is a response timeout.
    init(ackTimeout: UInt32, resendCount: Int) {
        ref = UnsafeMutablePointer<GG_CoapClientParameters>.allocate(capacity: 1)

        ref.pointee = GG_CoapClientParameters(
            ack_timeout: ackTimeout,
            max_resend_count: resendCount
        )
    }

    deinit {
        ref.deallocate()
    }
}

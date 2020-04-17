//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  CoapResponse+Response.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 5/30/18.
//

import GoldenGateXP

/// A CoAP response.
extension CoapServer {
    public struct Response {
        internal let responseCode: CoapCode.Response
        internal let options: [CoapOption]
        internal let token: RawCoapMessage.Token?
        internal let body: CoapOutgoingBody
        internal let messageId: RawCoapMessage.Identifier

        /// Creates a `GG_CoapMessage` from a `CoapResponse`.
        ///
        /// - Warning: Must be released manually!
        internal func asUnmanagedCoapMessage(type: CoapMessageType = .acknowledgement) throws -> OpaquePointer {
            let code = CoapCode.response(responseCode)
            let options = CoapMessageOptionParam.from(self.options)
            let token = self.token ?? Data()
            let payload = self.body.data ?? Data()

            var ref: OpaquePointer?
            try withExtendedLifetime(options) {
                try token.withUnsafeBytes { (tokenBytes: UnsafeRawBufferPointer) -> Void in
                    try payload.withUnsafeBytes { (payloadBytes: UnsafeRawBufferPointer) in
                        try GG_CoapMessage_Create(
                            code.rawValue,
                            type.ref,
                            options?.ref,
                            options?.count ?? 0,
                            messageId,
                            tokenBytes.baseAddress?.assumingMemoryBound(to: UInt8.self),
                            token.count,
                            payloadBytes.baseAddress?.assumingMemoryBound(to: UInt8.self),
                            payload.count,
                            &ref
                        ).rethrow()
                    }
                }
            }
            return ref!
        }
    }
}

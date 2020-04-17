//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  CoapRequestHandler.swift
//  GoldenGate
//
//  Created by Bogdan Vlad on 11/28/17.
//

import GoldenGateXP

/// A handler that can be registered to a CoapEndpoint.
/// It receives the requests and it will produce a CoAP response.
final class CoapRequestHandler: GGAdaptable {
    typealias GGObject = GG_CoapRequestHandler
    typealias GGInterface = GG_CoapRequestHandlerInterface

    let adapter: Adapter
    let responseHandler: CoapResponseHandler
    weak var endpoint: CoapEndpoint?

    private static var interface = GG_CoapRequestHandlerInterface(
        OnRequest: { ref, endpoint, requestPointer, _, _, responsePointer in
            .from {
                let handler = Adapter.takeUnretained(ref)

                guard let endpoint = handler.endpoint else {
                    LogBindingsError("CoAP endpoint got deallocated.")
                    throw GGRawError.failure
                }

                let rawRequest = try RawCoapMessage(message: requestPointer!)
                let responseBuilder = CoapResponseBuilder(request: rawRequest)
                let request = CoapMessage(
                    code: rawRequest.code,
                    options: rawRequest.options,
                    body: CoapStaticMessageBody(data: rawRequest.payload)
                )
                let response = try handler.responseHandler(request, responseBuilder)
                responsePointer?.pointee = try response.asUnmanagedCoapMessage()
            }
        }
    )

    /// Creates a handler.
    ///
    /// - Parameter responseHandler: The handler that will be called when a request is received
    ///                              by the CoAPEndpoint.
    /// - Parameter CoapResponse:     A struct that represents the request.
    /// - Returns: The response for the request received.
    init(responseHandler: @escaping CoapResponseHandler,
         endpoint: CoapEndpoint) {
        self.endpoint = endpoint
        self.responseHandler = responseHandler
        self.adapter = Adapter(iface: &type(of: self).interface)
        self.adapter.bind(to: self)
    }
}

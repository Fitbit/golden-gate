//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  CoapResponseBuilder.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 5/29/18.
//

import Foundation

public class CoapResponseBuilder: CoapMessageBuilder {
    public typealias Message = CoapServer.Response

    private let token: RawCoapMessage.Token?
    private let messageId: RawCoapMessage.Identifier
    private var options = [CoapOption]()
    private var responseCode = CoapCode.Response.success(.success)
    private var body = CoapOutgoingBody.none

    internal convenience init(request: RawCoapMessage) {
        self.init(messageId: request.messageId, token: request.token)
    }

    internal init(messageId: RawCoapMessage.Identifier, token: RawCoapMessage.Token?) {
        self.messageId = messageId
        self.token = token
    }

    @discardableResult
    public func option(number: CoapOption.Number, value: CoapOptionValue) -> Self {
        options.append(CoapOption(number: number, value: value))
        return self
    }

    @discardableResult
    public func body(data: Data, progress: ((Double) -> Void)? = nil) -> Self {
        self.body = .data(data, progress)
        return self
    }

    @discardableResult
    public func responseCode(_ responseCode: CoapCode.Response) -> Self {
        self.responseCode = responseCode
        return self
    }

    public func build() -> CoapServer.Response {
        return CoapServer.Response(
            responseCode: responseCode,
            options: options,
            token: token,
            body: body,
            messageId: messageId
        )
    }
}

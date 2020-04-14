//
//  RawCoapMessage.swift
//  GoldenGate
//
//  Created by Bogdan Vlad on 11/27/17.
//  Copyright © 2017 Fitbit. All rights reserved.
//

import Foundation
import GoldenGateXP
import RxSwift

enum RawCoapMessageError: Error {
    case unexpectedCode(code: UInt8)
}

/// An inbound CoAP message.
internal struct RawCoapMessage: Equatable {
    public typealias Identifier = UInt16
    public typealias Token = Data

    public let code: CoapCode
    public let type: CoapMessageType
    public let options: [CoapOption]
    public let token: Token?
    public let messageId: Identifier
    public let payload: Data?

    internal init(
        code: CoapCode,
        type: CoapMessageType,
        options: [CoapOption],
        token: Token?,
        payload: Data?,
        messageId: UInt16
    ) {
        self.code = code
        self.type = type
        self.options = options
        self.token = token
        self.payload = payload
        self.messageId = messageId
    }

    init(message: OpaquePointer) throws {
        // Get the token from the message
        var token = Data(count: Int(GG_COAP_MESSGAGE_MAX_TOKEN_LENGTH))

        let tokenLength = token.withUnsafeMutableBytes {
            GG_CoapMessage_GetToken(message, $0.baseAddress?.assumingMemoryBound(to: UInt8.self))
        }

        self.token = tokenLength > 0 ? token.prefix(Int(tokenLength)) : nil

        let rawCode = GG_CoapMessage_GetCode(message)
        guard let code = CoapCode(rawValue: rawCode) else {
            throw RawCoapMessageError.unexpectedCode(code: rawCode)
        }

        self.code = code

        self.type = CoapMessageType(value: GG_CoapMessage_GetType(message))
        self.messageId = GG_CoapMessage_GetMessageId(message)

        if
            let payloadRef = GG_CoapMessage_GetPayload(message),
            case let payloadSize = GG_CoapMessage_GetPayloadSize(message),
            payloadSize > 0
        {
            self.payload = Data(
                bytes: payloadRef,
                count: payloadSize
            )
        } else {
            self.payload = nil
        }

        // Get the options from the message
        var iterator: GG_CoapMessageOptionIterator = GG_CoapMessageOptionIterator()
        GG_CoapMessage_InitOptionIterator(message,
                                          UInt32(GG_COAP_MESSAGE_OPTION_ITERATOR_FILTER_ANY),
                                          &iterator)

        var mutableOptions: [CoapOption] = []

        while iterator.option.number != GG_COAP_MESSAGE_OPTION_NONE {
            guard let option = CoapOption(option: iterator.option) else {
                LogBindingsError("Wrong coap option. \(iterator.option)")
                continue
            }

            mutableOptions.append(option)
            GG_CoapMessage_StepOptionIterator(message, &iterator)
        }

        self.options = mutableOptions
    }
}

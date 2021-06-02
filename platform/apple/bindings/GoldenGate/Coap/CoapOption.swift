//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  CoapOption.swift
//  GoldenGate
//
//  Created by Bogdan Vlad on 11/22/17.
//

import Foundation
import GoldenGateXP

/// The options value formats according to CoAP specification (3.2 Option Value Formats).
public enum CoapOptionValue {
    case string(String)
    case opaque(Data)
    case uint(UInt32)
    case empty
}

extension CoapOptionValue: Equatable {
    static public func == (lhs: CoapOptionValue, rhs: CoapOptionValue) -> Bool {
        switch (lhs, rhs) {
        case let (.string(firstValue), .string(secondValue)):
            return firstValue == secondValue
        case let (.opaque(firstValue), .opaque(secondValue)):
            return firstValue == secondValue
        case let (.uint(firstValue), .uint(secondValue)):
            return firstValue == secondValue
        case (.empty, .empty):
            return true
        case (.string, _), (.opaque, _), (.uint, _), (.empty, _):
            return false
        }
    }
}

/// Prisms to read the payload
extension CoapOptionValue {
    var string: String? {
        guard case .string(let string) = self else { return nil }
        return string
    }

    var opaque: Data? {
        guard case .opaque(let opaque) = self else { return nil }
        return opaque
    }

    var uint: UInt32? {
        guard case .uint(let uint) = self else { return nil }
        return uint
    }
}

/// Each option instance in a message specifies the Option Number of the
/// defined CoAP option, the length of the Option Value, and the Option
/// Value itself. (Spec 3.1 Option Format)
public struct CoapOption: Equatable {
    let number: Number
    let value: CoapOptionValue

    public struct Number: RawRepresentable {
        public let rawValue: UInt32

        public init(rawValue: UInt32) {
            self.rawValue = rawValue
        }
    }

    public init(number: Number, value: CoapOptionValue) {
        self.number = number
        self.value = value
    }

    static public func == (lhs: CoapOption, rhs: CoapOption) -> Bool {
        return lhs.number == rhs.number && lhs.value == rhs.value
    }
}

public extension CoapOption.Number {
    static let ifMatch = CoapOption.Number(rawValue: 1)
    static let uriHost = CoapOption.Number(rawValue: 3)
    static let etag = CoapOption.Number(rawValue: 4)
    static let ifNoneMatch = CoapOption.Number(rawValue: 5)
    static let uriPort = CoapOption.Number(rawValue: 7)
    static let locationPath = CoapOption.Number(rawValue: 8)
    static let uriPath = CoapOption.Number(rawValue: 11)
    static let contentFormat = CoapOption.Number(rawValue: 12)
    static let maxAge = CoapOption.Number(rawValue: 14)
    static let uriQuery = CoapOption.Number(rawValue: 15)
    static let accept = CoapOption.Number(rawValue: 17)
    static let locationQuery = CoapOption.Number(rawValue: 20)
    static let block1 = CoapOption.Number(rawValue: 23)
    static let block2 = CoapOption.Number(rawValue: 27)
    static let size2 = CoapOption.Number(rawValue: 28)
    static let proxyUri = CoapOption.Number(rawValue: 35)
    static let proxyScheme = CoapOption.Number(rawValue: 39)
    static let size1 = CoapOption.Number(rawValue: 60)
}

/// Vendor specific options
public extension CoapOption.Number {
    static let startOffset = CoapOption.Number(rawValue: 2048)
}

extension CoapOption: CustomStringConvertible {
    public var description: String {
        return "CoapOption(\(number.rawValue)=\(value))"
    }
}

/// Transform from Swift to XP GG Coap Method.
extension CoapOption {
    init?(option: GG_CoapMessageOption) {
        self.number = CoapOption.Number(rawValue: option.number)

        switch option.type {
        case GG_COAP_MESSAGE_OPTION_TYPE_EMPTY:
            self.value = .empty
        case GG_COAP_MESSAGE_OPTION_TYPE_UINT:
            self.value = .uint(option.value.uint)
        case GG_COAP_MESSAGE_OPTION_TYPE_OPAQUE:
            self.value = .opaque(Data(bytes: option.value.opaque.bytes, count: Int(option.value.opaque.size)))
        case GG_COAP_MESSAGE_OPTION_TYPE_STRING:
            let data = Data(bytes: option.value.string.chars, count: Int(option.value.string.length))
            guard let string = String(data: data, encoding: .utf8) else {
                LogBindingsVerbose("Option could not be utf8 decoded.")
                return nil
            }

            self.value = .string(string)
        default:
            LogBindingsVerbose("Wrong option type \(option.type).")
            return nil
        }
    }
}

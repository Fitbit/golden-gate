//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  CoapMessageOptionParam.swift
//  GoldenGate
//
//  Created by Bogdan Vlad on 11/29/17.
//

import GoldenGateXP

/// An option parameter that can be followed by another parameter.
internal class CoapMessageOptionParam {
    typealias Ref = UnsafeMutablePointer<GG_CoapMessageOptionParam>
    let ref: Ref

    private let payload: NSData?

    public let next: CoapMessageOptionParam?

    /// Length of params chain.
    ///
    /// - Note: O(1) instead of O(n) when traversing via `next`
    public let count: Int

    /// Creates a new option parameter.
    ///
    /// - Parameters:
    ///   - option: The option to pass.
    ///   - next: The next option to pass.
    init(_ option: CoapOption, next: CoapMessageOptionParam? = nil) {
        let ggOption: GG_CoapMessageOption

        self.ref = UnsafeMutablePointer<GG_CoapMessageOptionParam>.allocate(capacity: 1)
        self.next = next
        self.count = (next?.count ?? 0) + 1

        switch option.value {
        case let .uint(number):
            self.payload = nil
            ggOption = GG_CoapMessageOption(
                number: option.number.rawValue,
                type: GG_COAP_MESSAGE_OPTION_TYPE_UINT,
                value: GG_CoapMessageOption.__Unnamed_union_value(uint: number)
            )
        case .empty:
            self.payload = nil
            ggOption = GG_CoapMessageOption(
                number: option.number.rawValue,
                type: GG_COAP_MESSAGE_OPTION_TYPE_EMPTY,
                value: GG_CoapMessageOption.__Unnamed_union_value()
            )
        case let .opaque(data):
            let data = data as NSData
            self.payload = data
            let bytesRef = data.bytes.assumingMemoryBound(to: UInt8.self)
            let value = GG_CoapMessageOption.__Unnamed_union_value.__Unnamed_struct_opaque(bytes: bytesRef, size: data.length)
            ggOption = GG_CoapMessageOption(
                number: option.number.rawValue,
                type: GG_COAP_MESSAGE_OPTION_TYPE_OPAQUE,
                value: GG_CoapMessageOption.__Unnamed_union_value(opaque: value)
            )
        case let .string(string):
            let data = string.data(using: .utf8)! as NSData
            self.payload = data
            let charsRef = data.bytes.assumingMemoryBound(to: Int8.self)
            let value = GG_CoapMessageOption.__Unnamed_union_value.__Unnamed_struct_string(chars: charsRef, length: data.length)
            ggOption = GG_CoapMessageOption(
                number: option.number.rawValue,
                type: GG_COAP_MESSAGE_OPTION_TYPE_STRING,
                value: GG_CoapMessageOption.__Unnamed_union_value(string: value)
            )
        }

        self.ref.pointee = GG_CoapMessageOptionParam(
            option: ggOption,
            next: (next?.ref).map(UnsafeMutablePointer.init),
            sorted_next: nil
        )
    }

    deinit {
        ref.deallocate()
    }

    /// Creates `CoapMessageOptionParam` from a list of options.
    ///
    /// Easier to use this than to manually set up `next`.
    static func from(_ options: [CoapOption]) -> CoapMessageOptionParam? {
        /// Reduce right to maintain order.
        return options.reduceRight(nil as CoapMessageOptionParam?) { next, option in
            CoapMessageOptionParam(option, next: next)
        }
    }

    internal var option: CoapOption! {
        return CoapOption(option: ref.pointee.option)
    }
}

private extension Collection {
    /// Reduces the collection starting from the end.
    ///
    /// - See Also: Collection.reduce
    func reduceRight<Result>(_ initialResult: Result, _ nextPartialResult: (Result, Element) throws -> Result) rethrows -> Result {
        return try lazy.reversed().reduce(initialResult, nextPartialResult)
    }
}

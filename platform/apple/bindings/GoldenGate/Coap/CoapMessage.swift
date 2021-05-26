//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  CoapMessage.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 5/30/18.
//

import GoldenGateXP
import RxSwift

public class CoapMessage {
    public let code: CoapCode
    public let options: [CoapOption]
    public let body: CoapMessageBody

    public init(code: CoapCode, options: [CoapOption], body: CoapMessageBody) {
        self.code = code
        self.options = options
        self.body = body
    }
}

enum CoapMessageBodyError: Error {
    case bodyAlreadyUsed
}

public protocol CoapMessageBody {
    func asData() -> Single<Data>
    func asStream() -> Observable<Data>
}

extension CoapMessage: CustomStringConvertible {
    public var description: String {
        return "CoapMessage: Code \(code) options: \(options)"
    }
}

public struct CoapExtendedError: Equatable {
    /// Error code
    public let code: Int32

    /// Error message, may be nil
    public let message: String?

    /// Error namespace (e.g. "org.example.foo")
    public let namespace: String

    public init(code: Int32, message: String? = nil, namespace: String) {
        self.code = code
        self.message = message
        self.namespace = namespace
    }

    /// Creates a CoapExtendedError from the given XP extended error.
    ///
    /// - Parameter error: XP extended error.
    init?(_ error: GG_CoapExtendedError) {
        guard let namespace = String(cString: error.name_space, size: error.name_space_size) else {
            return nil
        }
        self.namespace = namespace
        self.code = error.code
        self.message = String(cString: error.message, size: error.message_size)
    }

    /// Create a CoapExtendedError from a payload.
    ///
    /// - Parameter data: Payload to extract extended error from.
    init?(_ data: Data) {
        var xpExtendedError = GG_CoapExtendedError()
        let ggResult: GG_Result = data.withUnsafeBytes { (bytes: UnsafeRawBufferPointer) in
            guard let baseAddress = bytes.baseAddress, !bytes.isEmpty
            else { return GG_ERROR_INVALID_PARAMETERS }

            return GG_CoapExtendedError_Decode(
                &xpExtendedError,
                baseAddress.assumingMemoryBound(to: UInt8.self),
                data.count
            )
        }
        guard ggResult == GG_SUCCESS else { return nil }

        self.init(xpExtendedError)
    }

    /// Create a CoapExtendedError from a payload.
    ///
    /// - Parameter data: Payload to extract extended error from.
    init?(_ rawCoapMessage: RawCoapMessage) {
        guard let payload = rawCoapMessage.payload else {
            return nil
        }
        self.init(payload)
    }

    /// Return a protobuf encoded payload
    public var data: Data? {
        let ref = UnsafeMutablePointer<GG_CoapExtendedError>.allocate(capacity: 1)
        let namespace = self.namespace.data(using: .utf8)! as NSData
        let message = self.message?.data(using: .utf8)! as NSData?

        ref.pointee = GG_CoapExtendedError(
            name_space: namespace.bytes.assumingMemoryBound(to: Int8.self),
            name_space_size: namespace.count,
            code: code,
            message: message?.bytes.assumingMemoryBound(to: Int8.self),
            message_size: message?.count ?? 0
        )

        let size = GG_CoapExtendedError_GetEncodedSize(ref)
        var data = Data(count: size)
        let ggResult: GG_Result = data.withUnsafeMutableBytes {
            GG_CoapExtendedError_Encode(ref, $0.baseAddress?.assumingMemoryBound(to: UInt8.self))
        }

        ref.deallocate()

        return ggResult == GG_SUCCESS ? data : nil
    }
}

extension CoapExtendedError: CustomStringConvertible {
    public var description: String {
        return "CoapExtendedError: Code: \(code) Namespace: \(namespace)]"
    }
}

public extension CoapMessage {
    /// Observable that emits the current CoapExtendedError.
    var extendedError: Single<CoapExtendedError?> {
        return body.asData()
            .map { CoapExtendedError($0) }
            .catchErrorJustReturn(nil)
    }
}

private extension String {
    /// Creates a new string by copying the null-terminated UTF-8 data
    /// referenced by the given pointer (if size == 0) or creates a new string
    /// that contains a given number of bytes (if size > 0).
    ///
    /// - Parameters:
    ///   - cString: Pointer to UTF-8 data (may not be NULL terminated).
    ///   - size: Size of the message, or 0 if the string is null-terminated.
    init?(cString: UnsafePointer<Int8>!, size: Int) {
        guard cString != nil else { return nil }

        switch size {
        case 0:
            self.init(cString: cString)
        case 1...:
            let data = Data(bytes: cString, count: size)
            self.init(data: data, encoding: .utf8)
        default:
            return nil
        }
    }
}

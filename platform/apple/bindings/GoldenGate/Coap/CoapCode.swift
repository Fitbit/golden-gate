//
//  CoapCode.swift
//  GoldenGate
//
//  Created by Bogdan Vlad on 11/27/17.
//  Copyright © 2017 Fitbit. All rights reserved.
//

import GoldenGateXP

/// 8-bit unsigned integer, split into a 3-bit class (most
/// significant bits) and a 5-bit detail (least significant bits),
/// documented as "c.dd" where "c" is a digit from 0 to 7 for the
/// 3-bit subfield and "dd" are two digits from 00 to 31 for the 5-bit
/// subfield. (spec 3. Message Format)
public enum CoapCode: RawRepresentable, Equatable {
    public typealias RawValue = UInt8

    case request(Request)
    case response(Response)

    enum CodeClass: RawValue, Equatable {
        case request = 0b000_00000 // 0.xx
        case successfulResponse = 0b010_00000 // 2.xx
        case clientErrorResponse = 0b100_00000 // 4.xx
        case serverErrorResponse = 0b101_00000 // 5.xx
        
        static let mask: RawValue = 0b111_00000
    }

    public enum Request: RawValue, Equatable, CustomStringConvertible {
        case get = 1
        case post = 2
        case put = 3
        case delete = 4

        public var description: String {
            switch self {
            case .get: return "GET"
            case .post: return "POST"
            case .put: return "PUT"
            case .delete: return "DELETE"
            }
        }
    }

    public enum Response: Equatable {
        case success(SuccessfulResponse)
        case clientError(ClientErrorResponse)
        case serverError(ServerErrorResponse)

        static public func == (lhs: Response, rhs: Response) -> Bool {
            switch (lhs, rhs) {
            case let (.success(lhs), .success(rhs)): return lhs == rhs
            case let (.clientError(lhs), .clientError(rhs)): return lhs == rhs
            case let (.serverError(lhs), .serverError(rhs)): return lhs == rhs
            case (.success, _), (.clientError, _), (.serverError, _): return false
            }
        }
    }

    public enum SuccessfulResponse: Equatable {
        case success
        case created
        case deleted
        case valid
        case changed
        case content
        case unknown(RawValue)
    }

    public enum ClientErrorResponse: Equatable {
        case badRequest
        case unauthorized
        case badOption
        case forbidden
        case notFound
        case methodNotAllowed
        case notAcceptable
        case requestEntityIncomplete
        case preconditionFailed
        case requestEntityTooLarge
        case unsupportedContentFormat
        case unknown(RawValue)
    }

    public enum ServerErrorResponse: Equatable {
        case internalServerError
        case notImplemented
        case badGateway
        case serviceUnavailable
        case gatewayTimeout
        case proxyingNotSupported
        case unknown(RawValue)
    }

    public init?(rawValue: RawValue) {
        let classValue = rawValue & CodeClass.mask
        guard let codeClass = CodeClass(rawValue: classValue) else { return nil }

        let detailValue = rawValue & ~CodeClass.mask

        switch codeClass {
        case .request:
            guard let request = Request(rawValue: detailValue) else { return nil }
            self = .request(request)
        case .successfulResponse:
            let successfulResponse = SuccessfulResponse(rawValue: detailValue)
            self = .response(.success(successfulResponse))
        case .clientErrorResponse:
            let clientErrorResponse = ClientErrorResponse(rawValue: detailValue)
            self = .response(.clientError(clientErrorResponse))
        case .serverErrorResponse:
            let serverErrorResponse = ServerErrorResponse(rawValue: detailValue)
            self = .response(.serverError(serverErrorResponse))
        }
    }

    var codeClass: CodeClass {
        switch self {
        case .request: return .request
        case .response(.success): return .successfulResponse
        case .response(.clientError): return .clientErrorResponse
        case .response(.serverError): return .serverErrorResponse
        }
    }

    private var detailRawValue: RawValue {
        switch self {
        case .request(let request): return request.rawValue
        case .response(.success(let response)): return response.rawValue
        case .response(.clientError(let response)): return response.rawValue
        case .response(.serverError(let response)): return response.rawValue
        }
    }

    public var rawValue: UInt8 {
        return detailRawValue | codeClass.rawValue
    }

    private var classCode: UInt8 {
        switch self {
        case .request:
            return 0
        case .response(.success):
            return 2
        case .response(.clientError):
            return 4
        case .response(.serverError):
            return 5
        }
    }

    private var detailsCode: UInt8 {
        switch self {
        case .request(let request):
            return request.rawValue
        case .response(.success(let response)):
            return response.rawValue
        case .response(.clientError(let response)):
            return response.rawValue
        case .response(.serverError(let response)):
            return response.rawValue
        }
    }

    public var httpEquivalentCode: Int {
        return Int(classCode) * 100 + Int(detailsCode)
    }
}

extension CoapCode: CustomStringConvertible {
    public var description: String {
        let detail: Any

        switch self {
        case .request(let request):
            detail = request
        case .response(.success(let response)):
            detail = response
        case .response(.clientError(let response)):
            detail = response
        case .response(.serverError(let response)):
            detail = response
        }

        let detailsCodeString = (detailsCode <= 9) ? "0\(detailsCode)" : "\(detailsCode)"
        return "\(type(of: self))(\(codeClass).\(detail)) (\(classCode).\(detailsCodeString))"
    }
}

extension CoapCode.SuccessfulResponse {
    init(rawValue: CoapCode.RawValue) {
        switch rawValue {
        case 0: self = .success
        case 1: self = .created
        case 2: self = .deleted
        case 3: self = .valid
        case 4: self = .changed
        case 5: self = .content
        default: self = .unknown(rawValue)
        }
    }

    var rawValue: CoapCode.RawValue {
        switch self {
        case .success: return 0
        case .created: return 1
        case .deleted: return 2
        case .valid: return 3
        case .changed: return 4
        case .content: return 5
        case .unknown(let value): return value
        }
    }
}

extension CoapCode.ClientErrorResponse {
    init(rawValue: CoapCode.RawValue) {
        switch rawValue {
        case 0: self = .badRequest
        case 1: self = .unauthorized
        case 2: self = .badOption
        case 3: self = .forbidden
        case 4: self = .notFound
        case 5: self = .methodNotAllowed
        case 6: self = .notAcceptable
        case 8: self = .requestEntityIncomplete
        case 12: self = .preconditionFailed
        case 13: self = .requestEntityTooLarge
        case 15: self = .unsupportedContentFormat
        default: self = .unknown(rawValue)
        }
    }

    var rawValue: CoapCode.RawValue {
        switch self {
        case .badRequest: return 0
        case .unauthorized: return 1
        case .badOption: return 2
        case .forbidden: return 3
        case .notFound: return 4
        case .methodNotAllowed: return 5
        case .notAcceptable: return 6
        case .requestEntityIncomplete: return 8
        case .preconditionFailed: return 12
        case .requestEntityTooLarge: return 13
        case .unsupportedContentFormat: return 15
        case .unknown(let value): return value
        }
    }
}

extension CoapCode.ServerErrorResponse {
    init(rawValue: CoapCode.RawValue) {
        switch rawValue {
        case 0: self = .internalServerError
        case 1: self = .notImplemented
        case 2: self = .badGateway
        case 3: self = .serviceUnavailable
        case 4: self = .gatewayTimeout
        case 5: self = .proxyingNotSupported
        default: self = .unknown(rawValue)
        }
    }

    var rawValue: CoapCode.RawValue {
        switch self {
        case .internalServerError: return 0
        case .notImplemented: return 1
        case .badGateway: return 2
        case .serviceUnavailable: return 3
        case .gatewayTimeout: return 4
        case .proxyingNotSupported: return 5
        case .unknown(let value): return value
        }
    }
}

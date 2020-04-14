//
//  GGErrorType.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 10/25/17.
//  Copyright © 2017 Fitbit. All rights reserved.
//

import Foundation
import GoldenGateXP

protocol GGErrorType: Error {
    var ggResult: GG_Result { get }
}

public struct GGRawError: GGErrorType {
    let ggResult: GG_Result

    init(_ ggResult: GG_Result) {
        precondition(ggResult != GG_SUCCESS)
        self.ggResult = ggResult
    }

    /// A generic internal error.
    ///
    /// - See Also: GG_ERROR_INTERNAL
    public static let `internal` = GGRawError(GG_ERROR_INTERNAL)

    /// A generic error.
    ///
    /// - See Also: GG_FAILURE
    public static let failure = GGRawError(GG_FAILURE)

    /// An error indicating that invalid parameters have been passed to a function.
    ///
    /// - See Also: GG_ERROR_INVALID_PARAMETERS
    public static let invalidParameters = GGRawError(GG_ERROR_INVALID_PARAMETERS)

    /// An error indicating that the attempted operation can't performed
    /// in the current state (of an object or a subsystem).
    ///
    /// - See Also: GG_ERROR_INVALID_STATE
    public static let invalidState = GGRawError(GG_ERROR_INVALID_STATE)

    /// An error indicating that a requested item could not be found.
    ///
    /// - See Also: GG_ERROR_NO_SUCH_ITEM
    public static let noSuchItem = GGRawError(GG_ERROR_NO_SUCH_ITEM)

    /// An error indicating that not enough space was available
    /// to perform the requested operation.
    ///
    /// - See Also: GG_ERROR_NOT_ENOUGH_SPACE
    public static let notEnoughSpace = GGRawError(GG_ERROR_NOT_ENOUGH_SPACE)

    /// An error indicating that the attempted operation is not supported
    /// (on this object or this system).
    ///
    /// - See Also: GG_ERROR_NOT_SUPPORTED
    public static let notSupported = GGRawError(GG_ERROR_NOT_SUPPORTED)

    /// An error indicating that the attempted operation did not complete
    /// but timed out instead.
    ///
    /// - See Also: GG_ERROR_TIMEOUT
    public static let timeout = GGRawError(GG_ERROR_TIMEOUT)

    /// An error indicating that the attempted operation would block
    /// the current thread but the object/subsystem is unwilling to do that.
    ///
    /// - See Also: GG_ERROR_WOULD_BLOCK
    public static let wouldBlock = GGRawError(GG_ERROR_WOULD_BLOCK)
}

extension GGRawError: CustomNSError {
    public static var errorDomain: String {
        return "com.fitbit.goldengate"
    }

    public var errorCode: Int {
        return Int(ggResult)
    }

    public var errorUserInfo: [String: Any] {
        return [:]
    }
}

extension GGRawError: Equatable {
    public static func == (lhs: GGRawError, rhs: GGRawError) -> Bool {
        return lhs.ggResult == rhs.ggResult
    }
}

extension GG_Result {
    static func from(closure: () throws -> Void) -> GG_Result {
        do {
            try closure()
            return GG_SUCCESS
        } catch let error as GGErrorType {
            return error.ggResult
        } catch {
            LogBindingsWarning("Specific error \(error) will be reported as generic GG_FAILURE")
            return GG_FAILURE
        }
    }

    func rethrow() throws {
        guard self != GG_SUCCESS else { return }
        throw GGRawError(self)
    }

    func check(
        file: StaticString = #file,
        function: StaticString = #function,
        line: UInt = #line
    ) {
        if self != GG_SUCCESS {
            LogBindingsError("Received GG result: \(self)", file: file, function: function, line: line)
        }
    }
}

//
//  Logger.swift
//  GoldenGate
//
//  Created by Sylvain Rebaud on 11/17/17.
//  Copyright © 2017 Fitbit. All rights reserved.
//

import Foundation

// swiftlint:disable function_parameter_count identifier_name

/// A struct to represent extensible arbitrary domain types,
/// intentionally not using enums.
/// See https://matt.diephouse.com/2017/12/when-not-to-use-an-enum/
public struct LoggerDomain {
    public let domain: String

    public init(_ domain: String) {
        self.domain = domain
    }
}

public extension LoggerDomain {
    static let xp = LoggerDomain("xp")
    static let bindings = LoggerDomain("bindings")
    static let bluetooth = LoggerDomain("bluetooth")
}

/// Simple logging interface for GoldenGate.
///
/// An application that wants GoldenGate to use its logging solution will need to provide a type that conforms to this
/// signature and assign it to `GoldenGateLogger.defaultLogger`.
public protocol Logger {
    /// Logs the given message (using StaticString parameters).
    ///
    /// A concrete implementation should take into account the given `flag`
    /// and log (or not) according to it and whatever other logic the logger wants to implement.
    ///
    /// - Parameters:
    ///   - message: The message to be logger.
    ///              Provided as a closure to avoid performing interpolation if the message is not going to be logged.
    ///   - level: The severity of the message.
    ///            A logger can use this flag to decide if to log or ignore this specific message.
    ///   - file: The file name of the file where the message was created.
    ///   - function: The function name where the message was created.
    ///   - line: The line number in the file where the message was created.
    ///   - domain: The domain of the message.
    ///             A logger can use this information to group logs under the same namespace.
    func log(_ message: @autoclosure () -> String, level: LogLevel, file: StaticString, function: StaticString, line: UInt, domain: LoggerDomain)

    /// Logs the given message (using regular String parameters).
    ///
    /// A concrete implementation should take into account the given `flag`
    /// and log (or not) according to it and whatever other logic the logger wants to implement.
    ///
    /// - Parameters:
    ///   - message: The message to be logger.
    ///              Provided as a closure to avoid performing interpolation if the message is not going to be logged.
    ///   - level: The severity of the message.
    ///            A logger can use this flag to decide if to log or ignore this specific message.
    ///   - file: The file name of the file where the message was created.
    ///   - function: The function name where the message was created.
    ///   - line: The line number in the file where the message was created.
    ///   - domain: The domain of the message.
    ///             A logger can use this information to group logs under the same namespace.
    func log(_ message: @autoclosure () -> String, level: LogLevel, file: String, function: String, line: UInt, domain: LoggerDomain)
}

/// The logger levels that are reported to the Logger conforming objects.
public enum LogLevel: UInt, Comparable {
    case error
    case warning
    case info
    case debug
    case verbose

    // Comparable
    public static func < (lhs: LogLevel, rhs: LogLevel) -> Bool {
        return lhs.rawValue < rhs.rawValue
    }
}

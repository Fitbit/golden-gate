//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  BluetoothConnectionLogger.swift
//  GoldenGate
//
//  Created by Sylvain Rebaud on 11/17/17.
//

import Foundation

// swiftlint:disable function_parameter_count

/// Namespace for assigning the default logger.
///
/// An enum with no cases cannot be created, but can be used as namespace for static variables, for example.
public enum BluetoothConnectionLogger {
    public static var defaultLoggerOptions = LoggerOptions(rawValue: 0)
    public static var defaultLogLevel = LogLevel.info

    /// The default logger that will be used for GoldenGate.
    ///
    /// Assign this variable to your own implementation of Logger to receive the log messages created by GoldenGate.
    ///
    /// By default, the logging will be done to the standard output with simple `print` statements.
    public static var defaultLogger: Logger! = SimplePrintLogger(loggerOptions: defaultLoggerOptions, logLevel: defaultLogLevel) {
        didSet {
            if defaultLogger == nil {
                defaultLogger = SimplePrintLogger(loggerOptions: defaultLoggerOptions, logLevel: defaultLogLevel)
            }
        }
    }
}

public struct LoggerOptions: OptionSet {
    public let rawValue: UInt8

    public init(rawValue: UInt8) {
        self.rawValue = rawValue
    }

    public static let filename = LoggerOptions(rawValue: 1 << 0)
    public static let line = LoggerOptions(rawValue: 1 << 1)
    public static let function = LoggerOptions(rawValue: 1 << 2)
    public static let all = LoggerOptions(rawValue: RawValue.max)
}

private struct SimplePrintLogger: Logger {
    let loggerOptions: LoggerOptions
    let logLevel: LogLevel

    fileprivate func log(_ message: @autoclosure () -> String, level: LogLevel, file: StaticString, function: StaticString, line: UInt, domain: LoggerDomain) {
        log(message(), level: level, file: String(describing: file), function: String(describing: function), line: line, domain: domain)
    }

    fileprivate func log(_ message: @autoclosure () -> String, level: LogLevel, file: String, function: String, line: UInt, domain: LoggerDomain) {
        // filter out logs if level is too high
        guard level <= logLevel else { return }

        let date = SimplePrintLogger.dateFormatter.string(from: Date())
        let paddedDate = date.withCString { String(format: "%-12s", $0) }
        let paddedDomain = domain.domain.withCString { String(format: "%-3s", $0) }

        var output = "\(paddedDate) |\(level.loggingInitial)| \(paddedDomain) | "
        if loggerOptions.contains(.filename) {
            output += "\((file as NSString).lastPathComponent) "
        }
        if loggerOptions.contains(.line) {
            output += ":\(line) "
        }
        if loggerOptions.contains(.function) {
            output += "( \(function) ) "
        }
        output += message()

        // swiftlint:disable:next no_print
        print("\(output)")
    }

    private static let dateFormatter: DateFormatter = {
        let dateFormatter = DateFormatter()
        dateFormatter.dateFormat = "HH:mm:ss.SSS"
        return dateFormatter
    }()
}

fileprivate extension LogLevel {
    var loggingInitial: String {
        switch self {
        case .error: return "E"
        case .warning: return "W"
        case .info: return "I"
        case .debug: return "D"
        case .verbose: return "V"
        }
    }
}

fileprivate extension StaticString {
    var lastPathComponent: String {
        return (String(describing: self) as NSString).lastPathComponent
    }
}

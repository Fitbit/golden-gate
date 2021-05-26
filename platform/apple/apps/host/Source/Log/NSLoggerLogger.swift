//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  NSLoggerLogger.swift
//  GoldenGateHost-iOS
//
//  Created by Sylvain Rebaud on 2/15/18.
//

import Foundation
import GoldenGate
import NSLogger
import UIKit

// swiftlint:disable function_parameter_count

struct NSLoggerLogger: GoldenGate.Logger {
    func log(_ message: @autoclosure () -> String, level: LogLevel, file: StaticString, function: StaticString, line: UInt, domain: LoggerDomain) {
        log(message(), level: level, file: String(describing: file), function: String(describing: function), line: line, domain: domain)
    }

    func log(_ message: @autoclosure () -> String, level: LogLevel, file: String, function: String, line: UInt, domain: LoggerDomain) {
        LogMessage_noFormat(file, Int(line), function, domain.domain, Int(level.rawValue), message())
    }
}

extension NSLoggerLogger {
    init(_ bonjourName: String) {
        LoggerSetOptions(nil, UInt32(options))
        LoggerSetupBonjour(nil, nil, bonjourName as CFString)
    }

    private var options: Int {
        let defaultOptions =
            kLoggerOption_BufferLogsUntilConnection |
            kLoggerOption_BrowseBonjour |
            kLoggerOption_BrowsePeerToPeer |
            kLoggerOption_BrowseOnlyLocalDomain |
            kLoggerOption_UseSSL |
            kLoggerOption_CaptureSystemConsole

        #if DEBUG
            return defaultOptions | kLoggerOption_LogToConsole
        #else
            return defaultOptions
        #endif
    }
}

//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  Log.swift
//  GoldenGate
//
//  Created by Sylvain Rebaud on 11/20/17.
//

import Foundation

func assertLogError(
    _ message: @autoclosure () -> String,
    file: StaticString = #file,
    function: StaticString = #function,
    line: UInt = #line,
    domain: LoggerDomain
) {
    BluetoothConnectionLogger.defaultLogger.log(message(), level: .error, file: file, function: function, line: line, domain: domain)
    assertionFailure(message(), file: file, line: line)
}

// swiftlint:disable:next identifier_name
func LogError(
    _ message: @autoclosure () -> String,
    file: StaticString = #file,
    function: StaticString = #function,
    line: UInt = #line,
    domain: LoggerDomain
) {
    BluetoothConnectionLogger.defaultLogger.log(message(), level: .error, file: file, function: function, line: line, domain: domain)
}

// swiftlint:disable:next identifier_name
func LogWarning(
    _ message: @autoclosure () -> String,
    file: StaticString = #file,
    function: StaticString = #function,
    line: UInt = #line,
    domain: LoggerDomain
) {
    BluetoothConnectionLogger.defaultLogger.log(message(), level: .warning, file: file, function: function, line: line, domain: domain)
}

// swiftlint:disable:next identifier_name
func LogInfo(
    _ message: @autoclosure () -> String,
    file: StaticString = #file,
    function: StaticString = #function,
    line: UInt = #line,
    domain: LoggerDomain
) {
    BluetoothConnectionLogger.defaultLogger.log(message(), level: .info, file: file, function: function, line: line, domain: domain)
}

// swiftlint:disable:next identifier_name
func LogDebug(
    _ message: @autoclosure () -> String,
    file: StaticString = #file,
    function: StaticString = #function,
    line: UInt = #line,
    domain: LoggerDomain
) {
    BluetoothConnectionLogger.defaultLogger.log(message(), level: .debug, file: file, function: function, line: line, domain: domain)
}

// swiftlint:disable:next identifier_name
func LogVerbose(
    _ message: @autoclosure () -> String,
    file: StaticString = #file,
    function: StaticString = #function,
    line: UInt = #line,
    domain: LoggerDomain
) {
    BluetoothConnectionLogger.defaultLogger.log(message(), level: .verbose, file: file, function: function, line: line, domain: domain)
}

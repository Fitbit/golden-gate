//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  Log+Bluetooth.swift
//  GoldenGate
//
//  Created by Emanuel Jarnea on 11/09/2019.
//

import Foundation

func assertLogBluetoothError(
    _ message: @autoclosure () -> String,
    file: StaticString = #file,
    function: StaticString = #function,
    line: UInt = #line
) {
    assertLogError(message(), file: file, function: function, line: line, domain: .bluetooth)
}

// The name matches the name in the main app
// swiftlint:disable:next identifier_name
func LogBluetoothError(
    _ message: @autoclosure () -> String,
    file: StaticString = #file,
    function: StaticString = #function,
    line: UInt = #line
) {
    LogError(message(), file: file, function: function, line: line, domain: .bluetooth)
}

// The name matches the name in the main app
// swiftlint:disable:next identifier_name
func LogBluetoothWarning(
    _ message: @autoclosure () -> String,
    file: StaticString = #file,
    function: StaticString = #function,
    line: UInt = #line
) {
    LogWarning(message(), file: file, function: function, line: line, domain: .bluetooth)
}

// The name matches the name in the main app
// swiftlint:disable:next identifier_name
func LogBluetoothInfo(
    _ message: @autoclosure () -> String,
    file: StaticString = #file,
    function: StaticString = #function,
    line: UInt = #line
) {
    LogInfo(message(), file: file, function: function, line: line, domain: .bluetooth)
}

// The name matches the name in the main app
// swiftlint:disable:next identifier_name
func LogBluetoothDebug(
    _ message: @autoclosure () -> String,
    file: StaticString = #file,
    function: StaticString = #function,
    line: UInt = #line
) {
    LogDebug(message(), file: file, function: function, line: line, domain: .bluetooth)
}

// The name matches the name in the main app
// swiftlint:disable:next identifier_name
func LogBluetoothVerbose(
    _ message: @autoclosure () -> String,
    file: StaticString = #file,
    function: StaticString = #function,
    line: UInt = #line
) {
    LogVerbose(message(), file: file, function: function, line: line, domain: .bluetooth)
}

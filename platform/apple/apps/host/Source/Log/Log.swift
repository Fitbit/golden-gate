//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  Log.swift
//  GoldenGateHost
//
//  Created by Emanuel Jarnea on 11/09/2019.
//

import GoldenGate

// swiftlint:disable:next identifier_name
func Log(
    _ message: @autoclosure () -> String,
    _ level: LogLevel,
    file: StaticString = #file,
    function: StaticString = #function,
    line: UInt = #line
) {
    GoldenGateLogger.defaultLogger.log(message(), level: level, file: file, function: function, line: line, domain: .ggHost)
}

private extension LoggerDomain {
    static let ggHost = LoggerDomain("gg-host")
}

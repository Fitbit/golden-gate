//
//  Log.swift
//  GoldenGateHost
//
//  Created by Emanuel Jarnea on 11/09/2019.
//  Copyright © 2019 Fitbit. All rights reserved.
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

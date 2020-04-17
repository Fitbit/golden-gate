//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  LogHandler.swift
//  GoldenGate
//
//  Created by Sylvain Rebaud on 11/17/17.
//

import GoldenGateXP

final class LogHandler: GGAdaptable {
    typealias GGObject = GG_LogHandler
    typealias GGInterface = GG_LogHandlerInterface

    private var release: (() -> Void)?

    let adapter: Adapter

    private static var interface = GG_LogHandlerInterface(
        Log: { ref, record in
            Adapter.takeUnretained(ref).log(record!.pointee)
        },
        Destroy: { ref in
            Adapter.takeUnretained(ref).release?()
        }
    )

    init() {
        self.adapter = GGAdapter(iface: &type(of: self).interface)
        adapter.bind(to: self)
    }

    fileprivate func log(_ record: GG_LogRecord) {
        // ignore non-string records
        if record.message_type != GG_LOG_MESSAGE_TYPE_STRING {
            return
        }

        // forward log record data to default logger
        GoldenGateLogger.defaultLogger.log(
            String(cString: record.message.assumingMemoryBound(to: CChar.self)),
            level: record.level.logLevel,
            file: String(cString: record.source_file),
            function: String(cString: record.source_function),
            line: UInt(record.source_line),
            domain: .xp
        )
    }

    fileprivate func retain() -> LogHandler {
        // Note: This intentionally creates a retain-cycle which is
        // resolved when `release` is called by the loop. 🎩
        release = { self.release = nil }
        return self
    }

    static func register() {
        GG_LogManager_SetPlatformHandlerFactory { (_, _, handler) -> GG_Result in
            let logHandler = LogHandler()
            handler?.pointee = logHandler.retain().ref
            return GG_SUCCESS
        }
    }
}

extension Int32 {
    var logLevel: LogLevel {
        if self >= 600 {
            return .error
        } else if self >= 500 {
            return .warning
        } else if self >= 400 {
            return .info
        } else if self >= 300 {
            return .debug
        } else {
            return .verbose
        }
    }
}

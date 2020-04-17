//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  GoldenGate.swift
//  GoldenGate
//
//  Created by Sylvain Rebaud on 11/17/17.
//

import Foundation
import GoldenGateXP

public struct GoldenGateInitializer {
    private static var initialized: Bool = false

    static public func initialize() throws {
        objc_sync_enter(self)
        defer { objc_sync_exit(self) }

        // Exit if already initialized
        guard initialized == false else { return }

        // Register our log handler first
        LogHandler.register()

        // Change log level from default (all) to info
        if getenv("GG_LOG_CONFIG") == nil {
            setenv("GG_LOG_CONFIG", "plist:.level=INFO", 0)
        }

        LogBindingsVerbose("Initializing GoldenGate \(GoldenGateVersion.shared)...")

        // Initialize GoldenGate sub system
        try GG_Module_Initialize().rethrow()

        initialized = true
    }
}

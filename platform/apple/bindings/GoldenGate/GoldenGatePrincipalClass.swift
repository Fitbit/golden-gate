//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  GoldenGatePrincipalClass.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 12/6/17.
//

@testable import GoldenGate

// swiftlint:disable force_try

/// Needed so that `GoldenGateInitializer.initialize` is initialized
/// before any test suite is run.
@objc(GoldenGatePrincipalClass)
class GoldenGatePrincipalClass: NSObject {
    override init() {

        // Calling in twice for 100% test coverage
        try! GoldenGateInitializer.initialize()
        try! GoldenGateInitializer.initialize()

        // Assume that main loop is a RunLoop
        GoldenGate.RunLoop.assumeMainThreadIsRunLoop()

        super.init()
    }
}

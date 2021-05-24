//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  StackType.swift
//  GoldenGate
//
//  Created by Emanuel Jarnea on 04/06/2020.
//

import Foundation

/// A stack that is likely to be a network stack.
public protocol StackType {
    /// Provides access to the top most socket of the stack
    /// depending on the stack configuration.
    var topPort: Port? { get }

    func start()

    func destroy()
}

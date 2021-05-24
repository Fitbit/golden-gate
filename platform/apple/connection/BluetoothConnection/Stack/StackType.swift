//
//  StackType.swift
//  GoldenGate
//
//  Created by Emanuel Jarnea on 04/06/2020.
//  Copyright © 2020 Fitbit. All rights reserved.
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

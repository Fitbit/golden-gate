//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  GGEventType.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 3/28/18.
//

import Foundation
import GoldenGateXP

extension GG_Event {
    /// Create an event using `GGEventType`.
    ///
    /// - Parameters:
    ///   - type: The type of the event.
    ///   - source: The source of the event.
    ///
    /// - See Also: `GG_Event()`
    init(type: GGEventType, source: UnsafeMutableRawPointer!) {
        self.init(type: type.rawValue.rawValue, source: source)
    }
}

/// An event type (backed by a Four Char Code).
struct GGEventType: RawRepresentable {
    typealias RawValue = GGFourCharCode
    let rawValue: RawValue
}

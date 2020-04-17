//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  StackPortId.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 3/28/18.
//

import GoldenGateXP

/// Wrapper for `GG_STACK_PORT_ID_*` values.
internal struct StackPortId: RawRepresentable {
    let rawValue: UInt32

    /// Refers to the *top* port of a stack element.
    ///
    /// - See Also: GG_STACK_PORT_ID_TOP
    static let top = StackPortId(rawValue: UInt32(bitPattern: GG_STACK_PORT_ID_TOP))

    /// Refers to the *bottom* port of a stack element.
    ///
    /// - See Also: GG_STACK_PORT_ID_BOTTOM
    static let bottom = StackPortId(rawValue: UInt32(bitPattern: GG_STACK_PORT_ID_BOTTOM))
}

//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  StackElementId.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 3/28/18.
//

import GoldenGateXP

/// Wrapper for `GG_STACK_ELEMENT_*` values.
internal struct StackElementId: RawRepresentable {
    let rawValue: UInt32

    /// Refers to the *top* element of a stack.
    ///
    /// - See Also: GG_STACK_ELEMENT_ID_TOP
    static let top = StackElementId(rawValue: UInt32(bitPattern: GG_STACK_ELEMENT_ID_TOP))

    /// Refers to the *bottom* element of a stack.
    ///
    /// - See Also: GG_STACK_ELEMENT_ID_BOTTOM
    static let bottom = StackElementId(rawValue: UInt32(bitPattern: GG_STACK_ELEMENT_ID_BOTTOM))
}

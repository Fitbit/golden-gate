//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  NilComparer.swift
//  GoldenGate
//
//  Created by Sylvain Rebaud on 1/16/19.
//

import Foundation

public func nilComparer(lhs: Any?, rhs: Any?) -> Bool {
    return lhs == nil && rhs == nil
}

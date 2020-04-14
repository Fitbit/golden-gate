//
//  NilComparer.swift
//  GoldenGate
//
//  Created by Sylvain Rebaud on 1/16/19.
//  Copyright © 2019 Fitbit. All rights reserved.
//

import Foundation

public func nilComparer(lhs: Any?, rhs: Any?) -> Bool {
    return lhs == nil && rhs == nil
}

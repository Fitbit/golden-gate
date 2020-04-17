//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  OptionalType.swift
//  GoldenGate-iOS
//
//  Created by Marcel Jackwerth on 11/20/17.
//

import Foundation

protocol OptionalType {
    associatedtype Wrapped
    func map<U>(_ mapper: (Wrapped) throws -> U) rethrows -> U?
    static func none() -> Self
    var value: Wrapped? { get }
}

extension Optional: OptionalType {
    static func none() -> Optional {
        return self.none
    }

    /// Cast `Optional<Wrapped>` to `Wrapped?`
    var value: Wrapped? {
        return self
    }
}

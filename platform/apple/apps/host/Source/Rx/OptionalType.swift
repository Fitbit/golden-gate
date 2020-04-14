//
//  OptionalType.swift
//  GoldenGate-iOS
//
//  Created by Marcel Jackwerth on 11/20/17.
//  Copyright © 2018 Fitbit. All rights reserved.
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

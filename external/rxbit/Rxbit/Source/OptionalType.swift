//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  OptionalType.swift
//  Rxbit
//
//  Created by Marcel Jackwerth on 11/20/17.
//

import Foundation

public protocol OptionalType {
	associatedtype Wrapped
	func map<U>(_ mapper: (Wrapped) throws -> U) rethrows -> U?
	static func none() -> Self
	func asOptional() -> Wrapped?
}

extension Optional: OptionalType {
	public static func none() -> Optional {
		return self.none
	}

	/// Cast `Optional<Wrapped>` to `Wrapped?`.
	public func asOptional() -> Wrapped? {
		return self
	}
}

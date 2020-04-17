//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  OptionalStringInterpolation.swift
//  GoldenGate
//
//  Created by Vlad-Mihai Corneci on 26/10/2019.
//

/// Better handling of String conversion for optionals
/// , which results in either "Optional(…)" or "nil"
/// https://oleb.net/blog/2016/12/optionals-string-interpolation/
///
/// `
/// var someValue: Int? = 5
/// print("The value is \(someValue ??? "unknown")")
/// → "The value is 5"
/// someValue = nil
/// print("The value is \(someValue ??? "unknown")")
/// → "The value is unknown"
/// `

infix operator ???: NilCoalescingPrecedence

public func ??? <T>(optional: T?, defaultValue: @autoclosure () -> String) -> String {
    switch optional {
    case let value?: return String(describing: value)
    case nil: return defaultValue()
    }
}

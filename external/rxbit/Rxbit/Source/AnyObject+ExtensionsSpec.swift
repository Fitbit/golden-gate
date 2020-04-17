//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  AnyObject+ExtensionsSpec.swift
//  Rxbit
//
//  Created by Sylvain Rebaud on 8/29/18.
//

import Nimble
import Quick
@testable import Rxbit

// swiftlint:disable:next superfluous_disable_command
// swiftlint:disable force_cast function_body_length

// A counter to verify data is memoized
private var invocations = 0

// A class to test that AnyObject can memoize data
private class Invocation {
	var value: Int

	init(value: Int) {
		self.value = value
	}
}

// Extend Memoization with custom methods to reuse on different types
fileprivate extension Memoization {
	func memoize(_ key: UnsafeRawPointer) -> String {
		return memoize(key) {
			let result = "\(invocations)"
			invocations += 1
			return result
		}
	}

	func memoize(_ key: UnsafeRawPointer) -> Int? {
		return memoize(key) {
			let result = invocations > 0 ? invocations : nil
			invocations += 1
			return result
		}
	}

	func memoize(_ key: UnsafeRawPointer ) -> Invocation {
		return memoize(key) {
			let result = Invocation(value: invocations)
			invocations += 1
			return result
		}
	}
}

private protocol AssociatedVars {
	var invocationStruct: String { get }
	var invocationEnum: Int? { get }
	var invocationClass: Invocation { get }
}

// Use NSNumber as test that NSObjects can memoize data
fileprivate extension NSNumber {
	private static var invocationStructKey: UInt8 = 0
	private static var invocationEnumKey: UInt8 = 0
	private static var invocationClassKey: UInt8 = 0

	var invocationStruct: String {
		return memoize(&NSNumber.invocationStructKey)
	}

	var invocationEnum: Int? {
		return memoize(&NSNumber.invocationEnumKey)
	}

	var invocationClass: Invocation {
		return memoize(&NSNumber.invocationClassKey)
	}
}

extension NSNumber: AssociatedVars {}

// Use Int as a test that Structs can memoize data.
fileprivate extension Int {
	private static var invocationStructKey: UInt8 = 0
	private static var invocationEnumKey: UInt8 = 0
	private static var invocationClassKey: UInt8 = 0

	var invocationStruct: String {
		return memoize(&Int.invocationStructKey)
	}

	var invocationEnum: Int? {
		return memoize(&Int.invocationEnumKey)
	}

	var invocationClass: Invocation {
		return memoize(&Int.invocationClassKey)
	}
}

extension Int: AssociatedVars {}
extension Int: Memoization {}

// Use Optional as a test that Enums can memoize data.

private var optionalInvocationStructKey: UInt8 = 0
private var optionalInvocationEnumKey: UInt8 = 0
private var optionalInvocationClassKey: UInt8 = 0

fileprivate extension Optional {
	var invocationStruct: String {
		return memoize(&optionalInvocationStructKey)
	}

	var invocationEnum: Int? {
		return memoize(&optionalInvocationEnumKey)
	}

	var invocationClass: Invocation {
		return memoize(&optionalInvocationClassKey)
	}
}

extension Optional: AssociatedVars {}
extension Optional: Memoization {}

private typealias AssociatedVarsClosure = () -> AssociatedVars

private class AnyObjectExtensionConfiguration: QuickConfiguration {
	override class func configure(_ configuration: Configuration) {
		sharedExamples("memoize") { (context: @escaping SharedExampleContext) in
			beforeEach {
				invocations = 0
			}

			it("caches value types") {
				let extended: AssociatedVars = (context()["extended"] as! AssociatedVarsClosure)()
				var invocation = extended.invocationStruct
				expect(invocation) == "0"
				invocation = "1"
				expect(extended.invocationStruct) == "0"
				expect(invocations) == 1
			}

			it("caches nil optional") {
				var extended: AssociatedVars = (context()["extended"] as! AssociatedVarsClosure)()
				expect(extended.invocationEnum).to(beNil())
				expect(extended.invocationEnum).to(beNil())
				expect(invocations) == 1

				// Verify releasing variables should clear the cache and remove the associated object
				let old = extended
				extended = (context()["extended"] as! AssociatedVarsClosure)()
				expect(extended) !== old
				expect(extended.invocationEnum) == 1
				expect(extended.invocationEnum) == 1
				expect(invocations) == 2
			}

			it("caches references") {
				let extended: AssociatedVars = (context()["extended"] as! AssociatedVarsClosure)()
				let invocation = extended.invocationClass
				expect(invocation.value) == 0
				invocation.value = 2
				expect(extended.invocationClass.value) == 2
				expect(invocations) == 1
			}
		}
	}
}

private class AnyObjectExtensionsSpec: QuickSpec {
    override func spec() {
		var closure: AssociatedVarsClosure!

		describe("on a class") {
			beforeEach {
				closure = { arc4random() as NSNumber }
			}

			itBehavesLike("memoize") { ["extended": closure!] }
		}

		describe("on a struct") {
			beforeEach {
				closure = { Int(arc4random()) }
			}

			itBehavesLike("memoize") { ["extended": closure!] }
		}

		describe("on a enum") {
			beforeEach {
				closure = { Int(arc4random()) as Int? }
			}

			itBehavesLike("memoize") { ["extended": closure!] }
		}
	}
}

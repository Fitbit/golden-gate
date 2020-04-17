//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  AnyObject+Extensions.swift
//  Rxbit
//
//  Created by Sylvain Rebaud on 8/29/18.
//

import Foundation
import RxSwift

/// A protocol for synchronizing access to a block of code
public protocol Synchronization {}

public extension Synchronization {
	/// A method that guarantees the closure is synchronized
	/// on self.
	///
	/// - Parameter closure: The closure to synchronize
	/// - Returns: The value returned by the closure
	func synchronize<T>(_ closure: () -> T) -> T {
		return _synchronize(self, closure: closure)
	}
}

/// Add Synchronization conformance to NSObject so one can do:
/// ````
/// class Foo: NSObject {
///   func bar() {
///     return synchronize {
///       return ....
///     }
///   }
/// }
/// ```
extension NSObject: Synchronization {}

public extension Reactive {
	/// Add `synchronize` method to Reactive extension so one can do
	/// ````
	/// extension Reactive where Base: Foo {
	///     public var bar: Bar {
	///         return synchronize {
	///             ...
	///             return foobar
	///         }
	///     }
	/// }
	/// ````
	/// - Parameter closure: The closure to synchronize
	/// - Returns: The value returned by the closure
	func synchronize<T>(_ closure: () -> T) -> T {
		return _synchronize(self.base, closure: closure)
	}
}

private func _synchronize<T>(_ object: Any, closure: () -> T) -> T {
	objc_sync_enter(object)
	defer { objc_sync_exit(object) }

	return closure()
}

/// A protocol for caching data on any objects, including enums and structs.
public protocol Memoization {}

/// Add Memoization conformance to NSObject so one can do
/// ````
/// class Foo: NSObject {
///     static var key: UInt8 = 0
///     var bar: Bar {
///         return memoize(&Foo.key) {
///             expensiveOperation()
///         }
///     }
/// }
/// ````
extension NSObject: Memoization {}

public extension Memoization {
	/// Cache the result of an expensive operation.
	///
	/// - Parameters:
	///   - key: A unique key to associate the result of work.
	///   - work: A closure to produce a value.
	/// - Returns: The cached value produced by work.
	func memoize<T>(_ key: UnsafeRawPointer, work: @escaping () -> T) -> T {
		return _memoize(self, key: key, work: work)
	}
}

public extension Reactive {
	/// Cache the result of an expensive operation.
	///
	/// Note: Add `memoize` method to Reactive extension so one can do:
	/// ````
	/// var key: UInt8 = 0
	/// extension Reactive where Base: Foo {
  	///     public var bar: Bar {
    ///         return memoize(&key) {
    ///              expensiveOperation()
    ///         }
    ///     }
    /// }
	/// ````
	///
	/// - Parameters:
	///   - key: A unique key to associate the result of work.
	///   - work: A closure to produce a value.
	/// - Returns: The cached value produced by work.
	func memoize<T>(_ key: UnsafeRawPointer, work: @escaping () -> T) -> T {
		return _memoize(self.base, key: key, work: work)
	}
}

/// A class that wraps non class types that cannot store associated objects
/// otherwise (such as Structs or Enums)
private final class AssociatedWrapper<T: Any>: NSObject, NSCopying {
	typealias Wrapped = T
	public let value: Wrapped

	init(_ value: Wrapped) {
		self.value = value
	}

	func copy(with zone: NSZone? = nil) -> Any {
		return type(of: self).init(value)
	}
}

extension AssociatedWrapper where T: NSCopying {
	func copy(with zone: NSZone? = nil) -> Any {
		return type(of: self).init(value.copy(with: zone) as! T) // swiftlint:disable:this force_cast
	}
}

/// Makes sure the instance T returned by work is only created once by caching it as an associated object
private func _memoize<T: Any>(_ object: Any, key: UnsafeRawPointer, work: @escaping () -> T) -> T {
	return _synchronize(object) {
		if let value = objc_getAssociatedObject(object, key) as? AssociatedWrapper<T> {
			return value.value
		} else if let value = objc_getAssociatedObject(object, key) {
			return value as! T // swiftlint:disable:this force_cast
		}

		let result = work()
		objc_setAssociatedObject(object, key, T.self is AnyClass ? result : AssociatedWrapper<T>(result), .OBJC_ASSOCIATION_RETAIN_NONATOMIC)
		return result
	}
}

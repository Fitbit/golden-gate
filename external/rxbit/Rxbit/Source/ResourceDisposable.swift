//
//  ResourceDisposable.swift
//  Rxbit
//
//  Created by Bogdan Vlad on 12/12/2018.
//  Copyright © 2018 Fitbit, Inc. All rights reserved.
//

import Foundation
import RxSwift

public class ResourceDisposable: Cancelable {
	public typealias DisposeAction = () -> Void

	private var resources: [AnyObject]?
	private let lock = NSRecursiveLock()
	private var disposeAction: DisposeAction?

	/// - returns: Was resource disposed.
	public var isDisposed: Bool {
		lock.lock(); defer { lock.unlock() }
		return disposeAction != nil
	}

	/// Constructs a new disposable with the given action used for disposal and an array of
	/// references that needs to be kept alive until the disposable is disposed.
	///
	/// - parameter resources: An array of references that needs to be kept alive until this disposable is disposed.
	/// - parameter disposeAction: Disposal action which will be run upon calling `dispose`.
	public init(resources: [AnyObject], _ disposeAction: @escaping DisposeAction = { }) {
		self.resources = resources
		self.disposeAction = disposeAction
#if TRACE_RESOURCES
		_ = Resources.incrementTotal()
#endif
	}

	/// Calls the disposal action if and only if the current instance hasn't been disposed yet.
	///
	/// After invoking disposal action, disposal action will be dereferenced.
	public func dispose() {
		lock.lock(); defer { lock.unlock() }
		if let action = disposeAction {
			disposeAction = nil
			resources = nil
			action()
		}
	}

	deinit {
#if TRACE_RESOURCES
		_ = Resources.decrementTotal()
#endif
	}
}

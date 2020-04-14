//
//  OnInterrupted.swift
//  Rxbit
//
//  Created by Bogdan Vlad on 06/07/2018.
//  Copyright © 2018 Fitbit, Inc. All rights reserved.
//

import Foundation
import RxSwift

// swiftlint:disable vertical_parameter_alignment

public extension ObservableType {
	/// Registers an action to take upon when the observable is disposed before completing.
	///
	/// - Parameter onInterrupted: The closure that will be called when interruption happens.
	func `do`(onInterrupted: @escaping () -> Void) -> Observable<Element> {
		return .deferred {
			let lock = NSLock()
			var finished = false

			return self.do(onError: { _ in
				lock.lock()
				finished = true
				lock.unlock()
			}, onCompleted: {
				lock.lock()
				finished = true
				lock.unlock()
			}, onDispose: {
				lock.lock()
				let inProgress = !finished
				lock.unlock()

				if inProgress {
					onInterrupted()
				}
			})
		}
	}

	func `do`(onNext: ((Element) throws -> Void)? = nil,
			  onError: ((Swift.Error) throws -> Void)? = nil,
			  onCompleted: (() throws -> Void)? = nil,
			  onSubscribe: (() -> Void)? = nil,
			  onSubscribed: (() -> Void)? = nil,
			  onDispose: (() -> Void)? = nil,
			  onInterrupted: @escaping () -> Void)
		-> Observable<Element> {
			return self
				.do(onNext: onNext,
					onError: onError,
					onCompleted: onCompleted,
					onSubscribe: onSubscribe,
					onSubscribed: onSubscribed,
					onDispose: onDispose)
				.do(onInterrupted: onInterrupted)
	}

    /**
     Subscribes an element handler, an error handler, a completion handler, an interruption handler
	 and disposed handler to an observable sequence.

     - parameter onNext: Action to invoke for each element in the observable sequence.
     - parameter onError: Action to invoke upon errored termination of the observable sequence.
     - parameter onCompleted: Action to invoke upon graceful termination of the observable sequence.
     - parameter onDisposed: Action to invoke upon any type of termination of sequence (if the sequence has
     gracefully completed, errored, or if the generation is canceled by disposing subscription).
	 - Parameter onInterrupted: Action to invoke upon cancellation of the observable sequence.
     - returns: Subscription object used to unsubscribe from the observable sequence.
     */
	func subscribe(onNext: ((Element) -> Void)? = nil,
				   onError: ((Swift.Error) -> Void)? = nil,
				   onCompleted: (() -> Void)? = nil,
				   onDisposed: (() -> Void)? = nil,
				   onInterrupted: @escaping () -> Void)
		-> Disposable {
			return self
				.do(onInterrupted: onInterrupted)
				.subscribe(onNext: onNext, onError: onError, onCompleted: onCompleted, onDisposed: onDisposed)
	}
}

public extension PrimitiveSequenceType where Trait == SingleTrait {
	/// Registers an action to take upon when the observable is disposed before finishing.
	///
	/// - Parameter onInterrupted: The block that will be called when interrupt happens.
	func `do`(onInterrupted: @escaping () -> Void) -> PrimitiveSequence<SingleTrait, Element> {
		return self.primitiveSequence
			.asObservable()
			.do(onInterrupted: onInterrupted)
			.asSingle()
	}

	func `do`(onSuccess: ((Element) throws -> Void)? = nil,
			  onError: ((Swift.Error) throws -> Void)? = nil,
			  onSubscribe: (() -> Void)? = nil,
			  onSubscribed: (() -> Void)? = nil,
			  onDispose: (() -> Void)? = nil,
			  onInterrupted: @escaping () -> Void)
		-> Single<Element> {
			return self
				.do(onSuccess: onSuccess,
					onError: onError,
					onSubscribe: onSubscribe,
					onSubscribed: onSubscribed,
					onDispose: onDispose)
				.do(onInterrupted: onInterrupted)
	}

    /**
     Subscribes a success handler, an error handler and an interruption handler for this sequence.

     - parameter onSuccess: Action to invoke for each element in the observable sequence.
     - parameter onError: Action to invoke upon errored termination of the observable sequence.
	 - Parameter onInterrupted: Action to invoke upon cancellation of the observable sequence.
     - returns: Subscription object used to unsubscribe from the observable sequence.
     */
	func subscribe(onSuccess: ((Element) -> Void)? = nil,
				   onError: ((Swift.Error) -> Void)? = nil,
				   onInterrupted: @escaping () -> Void)
		-> Disposable {
			return self
				.do(onInterrupted: onInterrupted)
				.subscribe(onSuccess: onSuccess, onError: onError)
	}
}

public extension PrimitiveSequence where Trait == CompletableTrait, Element == Swift.Never {
	/// Registers an action to take upon when the observable is disposed before finishing.
	///
	/// - Parameter onInterrupted: The block that will be called when interrupt happens.
	func `do`(onInterrupted: @escaping () -> Void) -> PrimitiveSequence<CompletableTrait, Element> {
		return self.primitiveSequence
			.asObservable()
			.do(onInterrupted: onInterrupted)
			.asCompletable()
	}

	func `do`(onError: ((Swift.Error) throws -> Void)? = nil,
			  onCompleted: (() throws -> Void)? = nil,
			  onSubscribe: (() -> Void)? = nil,
			  onSubscribed: (() -> Void)? = nil,
			  onDispose: (() -> Void)? = nil,
			  onInterrupted: @escaping () -> Void)
		-> Completable {
			return self
				.do(onError: onError,
					onCompleted: onCompleted,
					onSubscribe: onSubscribe,
					onSubscribed: onSubscribed,
					onDispose: onDispose)
				.do(onInterrupted: onInterrupted)
	}

	/**
	Subscribes a completion handler, an error handler and an interruption handler for this sequence.

	- parameter onCompleted: Action to invoke upon graceful termination of the observable sequence.
	- parameter onError: Action to invoke upon errored termination of the observable sequence.
	- Parameter onInterrupted: Action to invoke upon cancellation of the observable sequence.
	- returns: Subscription object used to unsubscribe from the observable sequence.
	*/
	func subscribe(onCompleted: (() -> Void)? = nil, onError: ((Swift.Error) -> Void)? = nil, onInterrupted: @escaping () -> Void) -> Disposable {
		return self
			.do(onInterrupted: onInterrupted)
			.subscribe(onCompleted: onCompleted, onError: onError)
	}
}

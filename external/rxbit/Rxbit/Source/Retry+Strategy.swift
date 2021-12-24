//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  Retry+Strategy.swift
//  Rxbit
//
//  Created by Sylvain Rebaud on 11/1/18.
//

import Foundation
import RxRelay
import RxSwift

public protocol RetryStrategy {
	/// Erase the history of failures, allowing the strategy to start afresh.
	func resetFailureHistory()

	/// Called when an error has terminated the observable.
	///
	/// - Parameter error: The error that caused termination.
	/// - Returns: The action that should be performed.
	func action(for error: Error) -> RetryStrategyAction
}

/// A configuration for RetryStrategyAction.delay
public struct RetryDelayConfiguration {
	/// A delay before retrying. DispatchTimeInterval.never for no delay.
	public let interval: DispatchTimeInterval

	/// Maximum number of retries.
	public let maxRetries: UInt

	/// A multiplier to apply to delay for each attempt. 0 for none.
	/// Note: must be positive.
	public let multiplier: Double

	/// An observable to retry immediately without further waiting when it emits a value.
	public let resume: Observable<CustomStringConvertible>?

	public init(interval: DispatchTimeInterval, maxRetries: UInt = .max, multiplier: Double = 0, resume: Observable<CustomStringConvertible>? = nil) {
		self.interval = interval
		self.maxRetries = maxRetries
		self.multiplier = multiplier
		self.resume = resume
	}
}

public enum RetryStrategyAction {
	/// Will stop retrying and forward the error.
	case error(Error)

	/// Will retry according to the specified configuration.
	case delay(RetryDelayConfiguration)

	/// Will stop retrying until a trigger emits.
	case suspendUntil(Single<Void>)
}

extension RetryStrategyAction {
	/// Transform the retry action into a Single expected by `retryWhen` operator.
	///
	/// - Parameters:
	///   - attempt: the current retry attempt
	///   - error: the current error to report if not retrying
	///   - scheduler: A scheduler to use for the .delay action.
	/// - Returns: A Single that triggers a retry according to the strategy action when used
	///            with `retryWhen` operator.
	fileprivate func asSingle(attempt: Int, error: Error, scheduler: SchedulerType) -> Single<Void> {
		switch self {
		case .error(let error):
			return .error(error)
		case let .delay(configuration):
			guard configuration.multiplier >= 0 else {
				assertionFailure("Invalid negative multiplier \(configuration.multiplier)")
				return .error(error)
			}

			// check number of attempts already made against max number of attempts if specified
			// and give up with error or wait for resume trigger if any
			guard attempt < configuration.maxRetries else {
				return configuration.resume?.take(1).asSingle()
					.map { _ in () } ?? Single.error(error)
			}

			// retry immediately if no delay specified
			guard configuration.interval != .never else {
				return Single.just(())
			}

			// calculate delay
			let timeInterval = configuration.interval.timeInterval * pow(1 + configuration.multiplier, Double(attempt))
			
			return Observable<Void>.merge(
				Observable.just(()).delay(.milliseconds(Int(timeInterval * 1000.0)), scheduler: scheduler),
				configuration.resume?.map { _ in () } ?? Observable.never()
			)
			.take(1)
			.asSingle()
		case .suspendUntil(let trigger):
			return trigger
		}
	}

	public func asSingle(scheduler: SchedulerType) -> Single<Void> {
		// The attempt number and error will not be used
		asSingle(attempt: Int.min, error: RxError.unknown, scheduler: scheduler)
	}
}

public extension ObservableConvertibleType {
	/// Repeats the source observable sequence on error according to a strategy.
	///
	/// - Parameters:
	///   - strategy: A strategy defining the retry action to perform given the error emitted.
	///   - scheduler: A scheduler to use for the delay action.
	/// - Returns: An observable sequence producing the elements of the given sequence repeatedly until it terminates successfully or is notified to error or complete.
	func retry(withStrategy strategy: RetryStrategy, scheduler: SchedulerType) -> Observable<Element> {
		return self.asObservable().retry { errors in
			errors.enumerated().flatMap { attempt, error -> Single<Void> in
				strategy.action(for: error).asSingle(attempt: attempt, error: error, scheduler: scheduler)
			}
		}
	}

	func retry(withAction action: RetryStrategyAction, scheduler: SchedulerType) -> Observable<Element> {
		return self.asObservable().retry { errors in
			errors.enumerated().flatMap { attempt, error -> Single<Void> in
				action.asSingle(attempt: attempt, error: error, scheduler: scheduler)
			}
		}
	}
}

extension DispatchTimeInterval {
	internal var timeInterval: TimeInterval {
		switch self {
		case let .seconds(value):
			return TimeInterval(value)
		case let .milliseconds(value):
			return TimeInterval(TimeInterval(value) / 1000.0)
		case let .microseconds(value):
			return TimeInterval(Int64(value) * Int64(NSEC_PER_USEC)) / TimeInterval(NSEC_PER_SEC)
		case let .nanoseconds(value):
			return TimeInterval(value) / TimeInterval(NSEC_PER_SEC)
		case .never:
			return .infinity
		@unknown default:
			fatalError("unkonwn case")
		}
	}
}

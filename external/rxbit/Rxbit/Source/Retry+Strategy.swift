//
//  Retry+Strategy.swift
//  Rxbit
//
//  Created by Sylvain Rebaud on 11/1/18.
//  Copyright © 2018 Fitbit, Inc. All rights reserved.
//

import Foundation
import RxSwift

public protocol RetryStrategy {
	/// Called when an error has terminated the observable.
	///
	/// - Parameter error: The error that caused termination.
	/// - Returns: The action that should be performed.
	func action(for error: Error) -> RetryStrategyAction
}

/// A configuration for RetryStrategyAction.delay
public struct RetryDelayConfiguration {
	/// A delay before retrying. DispatchTimeInterval.never for no delay.
	let interval: DispatchTimeInterval

	/// Maximum number of retries. 0 for none.
	let maxRetries: UInt

	/// A multiplier to apply to delay for each attempt. 0 for none.
	/// Note: must be positive.
	let multiplier: Double

	/// An observable to retry immediately without further waiting when it emits a value.
	let resume: Observable<Void>?

	public init(interval: DispatchTimeInterval, maxRetries: UInt, multiplier: Double = 0, resume: Observable<Void>? = nil) {
		self.interval = interval
		self.maxRetries = maxRetries
		self.multiplier = multiplier
		self.resume = resume
	}
}

/// A strategy that retries regardless of the error
/// up to maxAttempts.
public struct DefaultRetryStrategy: RetryStrategy {
	let retryConfiguration: RetryDelayConfiguration

	public func action(for error: Error) -> RetryStrategyAction {
		return .delay(configuration: retryConfiguration)
	}

	public init(retryConfiguration: RetryDelayConfiguration) {
		self.retryConfiguration = retryConfiguration
	}

	public init(interval: DispatchTimeInterval, maxRetries: UInt, multiplier: Double = 0, resume: Observable<Void>? = nil) {
		self.init(retryConfiguration: RetryDelayConfiguration(interval: interval, maxRetries: maxRetries, multiplier: multiplier, resume: resume))
	}
}

public enum RetryStrategyAction {
	/// Will stop retrying and forward the error.
	case error(error: Error)

	/// Will retry according to the specfified configuration.
	case delay(configuration: RetryDelayConfiguration)

	/// Will stop retrying until a `trigger` emits an element.
	case suspendUntil(trigger: Observable<Void>)
}

extension RetryStrategyAction {
	/// Transform the retry action into an observable expected by `retryWhen` operator.
	///
	/// - Parameters:
	///   - attempt: the current retry attempt
	///   - error: the current error to report if not retrying
	///   - scheduler: A scheduler to use for the .delay action.
	/// - Returns: An observable that triggers a retry according to the strategy action when used
	///            with `retryWhen` operator.
	fileprivate func asObservable(attempt: Int, error: Error, scheduler: SchedulerType) -> Observable<Void> {
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
			guard configuration.maxRetries > 0, attempt < configuration.maxRetries else {
				return configuration.resume ?? .error(error)
			}

			// retry immediately if no delay specified
			guard configuration.interval != .never else {
				return Observable.just(())
			}

			// calculate delay
			let timeinterval = Int(configuration.interval.timeInterval * pow(1 + configuration.multiplier, Double(attempt)))
			return Observable.merge(
				Observable.just(()).delay(.seconds(timeinterval), scheduler: scheduler),
				configuration.resume ?? Observable.never()
			)
			.take(1)
		case .suspendUntil(let trigger):
			return trigger
		}
	}
}

public extension ObservableConvertibleType {
	/// Repeats the source observable sequence on error according to a strategy.
	///
	/// - Parameters:
	///   - strategy: A strategy defining the retry action to perform given the error emitted.
	///   - scheduler: A scheduler to use for the delay action.
	/// - Returns: An observable sequence producing the elements of the given sequence repeatedly until it terminates successfully or is notified to error or complete.
	func retryWith(_ strategy: RetryStrategy, scheduler: SchedulerType) -> Observable<Element> {
		return self.asObservable().retryWhen { errors in
			errors.enumerated().flatMap { attempt, error -> Observable<Void> in
				strategy.action(for: error).asObservable(attempt: attempt, error: error, scheduler: scheduler)
			}
		}
	}

	func retryWith(_ action: RetryStrategyAction, scheduler: SchedulerType) -> Observable<Element> {
		return self.asObservable().retryWhen { errors in
			errors.enumerated().flatMap { attempt, error -> Observable<Void> in
				action.asObservable(attempt: attempt, error: error, scheduler: scheduler)
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

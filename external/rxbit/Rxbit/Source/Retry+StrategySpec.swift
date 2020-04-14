//
//  Retry+StrategySpec.swift
//  Rxbit
//
//  Created by Sylvain Rebaud on 11/2/18.
//  Copyright © 2018 Fitbit, Inc. All rights reserved.
//

import Quick
import RxSwift
import RxTest

@testable import Rxbit

// swiftlint:disable:next superfluous_disable_command
// swiftlint:disable function_body_length force_try

struct AlwaysErrorStrategy: RetryStrategy {
	func action(for error: Error) -> RetryStrategyAction {
		return .error(error: error)
	}
}

struct TriggerErrorStrategy: RetryStrategy {
	let trigger: Observable<Void>

	func action(for error: Error) -> RetryStrategyAction {
		return .suspendUntil(trigger: trigger)
	}
}

class RetryStrategySpec: QuickSpec {
	override func spec() {
		describe("retryWith") {
			it("errors immediately") {
				let factories: [(TestableObservable<Int>, SchedulerType) -> Observable<Int>] = [ {
					$0.retryWith(AlwaysErrorStrategy(), scheduler: $1)
				}, {
					$0.retryWith(DefaultRetryStrategy(interval: .seconds(10), maxRetries: 0, multiplier: 0), scheduler: $1)
				}, {
					$0.retryWith(.error(error: RetryError.dummyError), scheduler: $1)
				}, {
					$0.retryWith(.delay(configuration: RetryDelayConfiguration(interval: .seconds(10), maxRetries: 0)), scheduler: $1)
				}]

				for factory in factories {
					let scheduler = TestScheduler(initialClock: 0)

					let input = scheduler.createColdObservable([
						.next(10, 1),
						.next(20, 2),
						.error(30, RetryError.dummyError)
					])

					let res = scheduler.start(disposed: 300) {
						factory(input, scheduler)
					}

					XCTAssertEqual(res.events, [
						.next(210, 1),
						.next(220, 2),
						.error(230, RetryError.dummyError)
					])

					XCTAssertEqual(input.subscriptions, [
						Subscription(200, 230)
					])
				}
			}

			it("retries immediately") {
				let strategy = DefaultRetryStrategy(interval: .never, maxRetries: 2, multiplier: 0)
				let action = RetryStrategyAction.delay(configuration: strategy.retryConfiguration)

				let factories: [(TestableObservable<Int>, SchedulerType) -> Observable<Int>] = [ {
					$0.retryWith(strategy, scheduler: $1)
				}, {
					$0.retryWith(action, scheduler: $1)
				}]

				for factory in factories {
					let scheduler = TestScheduler(initialClock: 0)

					let input = scheduler.createColdObservable([
						.next(10, 1),
						.next(20, 2),
						.error(30, RetryError.dummyError)
						])

					let res = scheduler.start(disposed: 600) {
						factory(input, scheduler)
					}

					XCTAssertEqual(res.events, [
						.next(210, 1),
						.next(220, 2),
						.next(240, 1),
						.next(250, 2),
						.next(270, 1),
						.next(280, 2),
						.error(290, RetryError.dummyError)
					])

					XCTAssertEqual(input.subscriptions, [
						Subscription(200, 230),
						Subscription(230, 260),
						Subscription(260, 290)
					])
				}
			}

			it("retries up to a maximum") {
				let strategy = DefaultRetryStrategy(interval: .seconds(10), maxRetries: 2, multiplier: 0)
				let action = RetryStrategyAction.delay(configuration: strategy.retryConfiguration)

				let factories: [(TestableObservable<Int>, SchedulerType) -> Observable<Int>] = [ {
					$0.retryWith(strategy, scheduler: $1)
				}, {
					$0.retryWith(action, scheduler: $1)
				}]

				for factory in factories {
					let scheduler = TestScheduler(initialClock: 0)

					let input = scheduler.createColdObservable([
						.next(10, 1),
						.next(20, 2),
						.error(30, RetryError.dummyError)
					])

					let res = scheduler.start(disposed: 600) {
						factory(input, scheduler)
					}

					XCTAssertEqual(res.events, [
						.next(210, 1),
						.next(220, 2),
						.next(250, 1),
						.next(260, 2),
						.next(290, 1),
						.next(300, 2),
						.error(310, RetryError.dummyError)
					])

					XCTAssertEqual(input.subscriptions, [
						Subscription(200, 230),
						Subscription(240, 270),
						Subscription(280, 310)
					])
				}
			}

			it("applies a multiplier when retrying with a delay") {
				let strategy = DefaultRetryStrategy(interval: .seconds(10), maxRetries: 2, multiplier: 0.5)
				let action = RetryStrategyAction.delay(configuration: strategy.retryConfiguration)

				let factories: [(TestableObservable<Int>, SchedulerType) -> Observable<Int>] = [ {
					$0.retryWith(strategy, scheduler: $1)
				}, {
					$0.retryWith(action, scheduler: $1)
				}]

				for factory in factories {
					let scheduler = TestScheduler(initialClock: 0)

					let input = scheduler.createColdObservable([
						.next(0, 1),
						.next(10, 2),
						.error(20, RetryError.dummyError)
					])

					let res = scheduler.start(disposed: 300) {
						factory(input, scheduler)
					}

					XCTAssertEqual(res.events, [
						.next(200, 1),
						.next(210, 2),
						.next(230, 1),
						.next(240, 2),
						.next(265, 1),
						.next(275, 2),
						.error(285, RetryError.dummyError)
					])

					XCTAssertEqual(input.subscriptions, [
						Subscription(200, 220),
						Subscription(230, 250),
						Subscription(265, 285)
					])
				}
			}

			it("suspends retries until trigger fires") {
				let trigger = PublishSubject<Void>()
				let strategy = TriggerErrorStrategy(trigger: trigger)
				let action = RetryStrategyAction.suspendUntil(trigger: trigger)

				let factories: [(TestableObservable<Int>, SchedulerType) -> Observable<Int>] = [ {
					$0.retryWith(strategy, scheduler: $1)
				}, {
					$0.retryWith(action, scheduler: $1)
				}]

				for factory in factories {
					let scheduler = TestScheduler(initialClock: 0)

					let input = scheduler.createColdObservable([
						.next(0, 1),
						.next(10, 2),
						.error(20, RetryError.dummyError)
					])

					scheduler.scheduleAt(250) {
						trigger.onNext(())
					}

					let res = scheduler.start(disposed: 300) {
						factory(input, scheduler)
					}

					XCTAssertEqual(res.events, [
						.next(200, 1),
						.next(210, 2),
						.next(250, 1),
						.next(260, 2)
					])

					XCTAssertEqual(input.subscriptions, [
						Subscription(200, 220),
						Subscription(250, 270)
					])
				}
			}

			it("resumes retries earlier when trigger fires") {
				let trigger = PublishSubject<Void>()
				let strategy = DefaultRetryStrategy(interval: .seconds(100), maxRetries: 2, multiplier: 0, resume: trigger)
				let action = RetryStrategyAction.delay(configuration: strategy.retryConfiguration)

				let factories: [(TestableObservable<Int>, SchedulerType) -> Observable<Int>] = [ {
					$0.retryWith(strategy, scheduler: $1)
				}, {
					$0.retryWith(action, scheduler: $1)
				}]

				for factory in factories {
					let scheduler = TestScheduler(initialClock: 0)

					let input = scheduler.createColdObservable([
						.next(0, 1),
						.next(10, 2),
						.error(20, RetryError.dummyError)
					])

					scheduler.scheduleAt(250) {
						trigger.onNext(())
					}

					let res = scheduler.start(disposed: 300) {
						factory(input, scheduler)
					}

					XCTAssertEqual(res.events, [
						.next(200, 1),
						.next(210, 2),
						.next(250, 1),
						.next(260, 2)
					])

					XCTAssertEqual(input.subscriptions, [
						Subscription(200, 220),
						Subscription(250, 270)
					])
				}
			}
		}
	}
}

enum RetryError: Error {
	case dummyError
}

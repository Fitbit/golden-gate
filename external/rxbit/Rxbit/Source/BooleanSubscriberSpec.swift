//
//  BooleanSubscriberSpec.swift
//  Rxbit
//
//  Created by Marcel Jackwerth on 8/9/18.
//  Copyright © 2018 Fitbit. All rights reserved.
//

import Nimble
import Quick
@testable import Rxbit
import RxBlocking
import RxSwift

// swiftlint:disable:next superfluous_disable_command
// swiftlint:disable function_body_length force_try

class BooleanSubscriberSpec: QuickSpec {
	override func spec() {
		var subscriptionCount: Int = 0
		var isSubscribed: Bool!
		var source: ((CompletableEvent) -> Void)!
		var booleanSubscriber: BooleanSubscriber!

		beforeEach {
			subscriptionCount = 0
			isSubscribed = false
			source = { _ in }
			booleanSubscriber = BooleanSubscriber(source: Completable.create { observer in
				source = observer
				subscriptionCount += 1
				isSubscribed = true

				return Disposables.create {
					source = { _ in }
					isSubscribed = false
				}
			})
		}

		describe("accept()") {
			it("subscribes to the source") {
				booleanSubscriber.accept(true)

				expect(subscriptionCount) == 1
				expect(isSubscribed) == true
			}

			it("unsubscribes to the source") {
				booleanSubscriber.accept(true)
				booleanSubscriber.accept(false)

				expect(subscriptionCount) == 1
				expect(isSubscribed) == false
			}

			it("doesn't unnecessarily re-subscribe to the source") {
				booleanSubscriber.accept(true)
				booleanSubscriber.accept(true)

				expect(subscriptionCount) == 1
				expect(isSubscribed) == true
			}

			it("re-subscribes if the source completed") {
				booleanSubscriber.accept(true)
				source(.completed)
				booleanSubscriber.accept(true)

				expect(subscriptionCount) == 2
				expect(isSubscribed) == true
			}

			it("re-subscribes if the source errored") {
				booleanSubscriber.accept(true)
				source(.error(SpecError.someError))
				booleanSubscriber.accept(true)

				expect(subscriptionCount) == 2
				expect(isSubscribed) == true
			}
		}

		describe("subscribed") {
			it("reports true if subscribed") {
				booleanSubscriber.accept(true)

				expect(booleanSubscriber.subscribedValue) == true
			}

			it("reports false if source completed") {
				booleanSubscriber.accept(true)
				source(.completed)

				expect(booleanSubscriber.subscribedValue) == false
			}

			it("reports false if source errored") {
				booleanSubscriber.accept(true)
				source(.error(SpecError.someError))

				expect(booleanSubscriber.subscribedValue) == false
			}

			it("reports true when resubscribing after source completed") {
				booleanSubscriber.accept(true)
				source(.completed)
				booleanSubscriber.accept(true)

				expect(booleanSubscriber.subscribedValue) == true
			}

			it("reports true when resubscribing after source completed") {
				booleanSubscriber.accept(true)
				source(.error(SpecError.someError))
				booleanSubscriber.accept(true)

				expect(booleanSubscriber.subscribedValue) == true
			}
		}
	}
}

private enum SpecError: Error {
	case someError
}

private extension BooleanSubscriber {
	var subscribedValue: Bool {
		// Never fails, never completes
		return try! subscribed.toBlocking().first()!
	}
}

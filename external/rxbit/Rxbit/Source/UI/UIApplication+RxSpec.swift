//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  UIApplication+RxSpec.swift
//  Rxbit
//
//  Created by Ryan LaSante on 8/31/18.
//

#if os(iOS) || os(tvOS)

// swiftlint:disable force_try function_body_length

import Nimble
import Quick
@testable import Rxbit
import RxBlocking
import RxSwift

private class MockApplicationStateProvider: ReactiveCompatible, UIApplicationStateProviderType {
	var applicationState: UIApplication.State = .active {
		didSet {
			switch (oldValue, applicationState) {
			case (.inactive, .active), (.background, .active):
				NotificationCenter.default.post(name: UIApplication.didBecomeActiveNotification, object: nil)
			case (.active, .background), (.inactive, .background):
				NotificationCenter.default.post(name: UIApplication.didEnterBackgroundNotification, object: nil)
			case (.active, .inactive):
				NotificationCenter.default.post(name: UIApplication.willResignActiveNotification, object: nil)
			case (.background, .inactive):
				NotificationCenter.default.post(name: UIApplication.willEnterForegroundNotification, object: nil)
			default:
				// Do nothing if we're not changing state
				break
			}
		}
	}
}

private class UIApplicationSpec: QuickSpec {
	override func spec() {
		var applicationStateProvider: MockApplicationStateProvider!
		var disposeBag = DisposeBag()
		var applicationState: ReplaySubject<UIApplication.State>!
		var applicationStateActive: ReplaySubject<Bool>!
		var applicationBecameActive: ReplaySubject<Void>!

		beforeEach {
			applicationStateProvider = MockApplicationStateProvider()
			disposeBag = DisposeBag()

			applicationState = ReplaySubject.createUnbounded()
			applicationStateProvider.rx.applicationState
				.bind(to: applicationState)
				.disposed(by: disposeBag)

			applicationStateActive = ReplaySubject.createUnbounded()
			applicationStateProvider.rx.applicationStateActive
				.bind(to: applicationStateActive)
				.disposed(by: disposeBag)

			applicationBecameActive = ReplaySubject.createUnbounded()
			applicationStateProvider.rx.applicationBecameActive
				.bind(to: applicationBecameActive)
				.disposed(by: disposeBag)
		}

		it("returns current application state") {
			let applicationState = try! applicationStateProvider.rx.applicationState.toBlocking().first()
			expect(applicationState) == applicationStateProvider.applicationState
		}

		it("returns new values on notifications") {
			applicationStateProvider.applicationState = .background
			applicationState.on(.completed)
			let states = try! applicationState.asObservable().toBlocking().toArray()
			expect(states.count) == 2
			expect(states.last) == applicationStateProvider.applicationState
		}

		it("does not send duplicate events") {
			NotificationCenter.default.post(name: UIApplication.didEnterBackgroundNotification, object: nil)
			applicationState.on(.completed)
			let states = try! applicationState.asObservable().toBlocking().toArray()
			expect(states.count) == 1
		}

		it("does not cache invalid start values") {
			let observer = applicationStateProvider.rx.applicationState
			// Change state before subscription
			applicationStateProvider.applicationState = .background
			let applicationState = try! observer.toBlocking().first()
			expect(applicationState) == applicationStateProvider.applicationState
		}

		it("emits distinct application active states") {
			applicationStateProvider.applicationState = .background
			applicationStateProvider.applicationState = .background
			applicationStateProvider.applicationState = .active
			applicationStateProvider.applicationState = .inactive
			applicationStateActive.on(.completed)
			let states = try! applicationStateActive.asObservable().toBlocking().toArray()

			let expectedStates = [true, false, true, false]
			expect(states) == expectedStates
		}

		it("emits when application becomes active") {
			applicationStateProvider.applicationState = .background
			applicationStateProvider.applicationState = .active
			applicationStateProvider.applicationState = .active
			applicationBecameActive.on(.completed)
			let states = try! applicationBecameActive.asObservable().toBlocking().toArray()

			expect(states.count) == 1
		}
	}
}

#endif

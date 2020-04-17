//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  SingleUseCacheSubjectSpec.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 6/1/18.
//

import Nimble
import Quick
@testable import Rxbit
import RxCocoa
import RxSwift
import RxTest

// swiftlint:disable function_body_length
class SingleUseCacheSubjectSpec: QuickSpec {
	override func spec() {
		var cache: SingleUseCacheSubject<Int>!

		var scheduler: TestScheduler!
		var disposeBag: DisposeBag!

		beforeEach {
			cache = SingleUseCacheSubject<Int>()

			scheduler = TestScheduler(initialClock: 0)
			disposeBag = DisposeBag()
		}

		it("caches emitted elements before subscription") {
			let cacheObserver = scheduler.createObserver(Int.self)
			cache.on(.next(40))
			cache.on(.next(41))
			cache.on(.next(42))

			cache
				.asObservable()
				.subscribe(cacheObserver)
				.disposed(by: disposeBag)

			expect(cacheObserver.events).to(haveCount(3))
			expect(cacheObserver.events[0].value.element) == 40
			expect(cacheObserver.events[1].value.element) == 41
			expect(cacheObserver.events[2].value.element) == 42
		}

		it("caches emitted elements and continues emitting new ones") {
			let cacheObserver = scheduler.createObserver(Int.self)
			cache.on(.next(40))
			cache.on(.next(41))
			cache.on(.next(42))

			cache
				.asObservable()
				.subscribe(cacheObserver)
				.disposed(by: disposeBag)

			cache.on(.next(43))
			cache.on(.next(44))
			cache.on(.next(45))

			expect(cacheObserver.events).to(haveCount(6))
			expect(cacheObserver.events[0].value.element) == 40
			expect(cacheObserver.events[1].value.element) == 41
			expect(cacheObserver.events[2].value.element) == 42
			expect(cacheObserver.events[3].value.element) == 43
			expect(cacheObserver.events[4].value.element) == 44
			expect(cacheObserver.events[5].value.element) == 45
		}

		it("waits for a value") {
			waitUntil { done in
				_ = cache.asSingle().subscribe(onSuccess: { _ in
					done()
				})

				cache.on(.next(42))
				cache.on(.completed)
			}
		}

		it("waits for an observer") {
			cache.on(.next(42))
			cache.on(.completed)

			waitUntil { done in
				_ = cache.asSingle().subscribe(onSuccess: { _ in
					done()
				})
			}
		}

		it("reports errors") {
			cache.on(.error(SingleUseCacheSpecError.dummy))

			waitUntil { done in
				_ = cache.asSingle().subscribe(onError: { error in
					expect(error).to(matchError(SingleUseCacheSpecError.dummy))
					done()
				})
			}
		}

		it("reports 'already used' error if it was consumed previously") {
			waitUntil { done in
				_ = cache.asSingle().subscribe()
				_ = cache.asSingle().subscribe(onError: { error in
					expect(error).to(matchError(SingleUseCacheSubjectError.alreadyUsed))
					done()
				})
			}
		}

		it("reports 'already used' error if it was consumed previously") {
			waitUntil { done in
				_ = cache.asObservable().subscribe()
				_ = cache.asSingle().subscribe(onError: { error in
					expect(error).to(matchError(SingleUseCacheSubjectError.alreadyUsed))
					done()
				})
			}
		}

		it("rejects additional observers") {
			waitUntil { done in
				_ = cache.asSingle().subscribe()
				_ = cache.asSingle().subscribe(onError: { _ in
					done()
				})
			}
		}

		it("releases cached value once consumed") {
			let cache = SingleUseCacheSubject<DeinitTester>()
			_ = cache.asSingle().subscribe()

			waitUntil { done in
				cache.on(.next(DeinitTester(onDeinit: done)))
				cache.on(.completed)
			}
		}

		it("doesn't lock when calling lock recursively") {
			cache.on(.next(42))
			cache.on(.completed)

			waitUntil(action: { done in
				_ = cache.asSingle().subscribe(onSuccess: { _ in
					cache.on(.next(43))
					cache.on(.completed)
					done()
				})
			})
		}
	}
}

private enum SingleUseCacheSpecError: Error {
	case dummy
}

private class DeinitTester {
	let onDeinit: () -> Void

	init(onDeinit: @escaping () -> Void) {
		self.onDeinit = onDeinit
	}

	deinit {
		onDeinit()
	}
}

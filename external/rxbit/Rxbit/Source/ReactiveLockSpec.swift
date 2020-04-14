//
//  ReactiveLockSpec.swift
//  Devicebit
//
//  Created by Bogdan Vlad on 18/01/2019.
//  Copyright © 2019 Fitbit, Inc. All rights reserved.
//

import Nimble
import Quick
@testable import Rxbit
import RxSwift
import RxTest

// swiftlint:disable trailing_closure function_body_length

class ReactiveLockSpec: QuickSpec {
	override func spec() {
		describe("acquiring reactiveLock") {
			it("successfully acquires the lock when no one is on it") {
				let scheduler = TestScheduler(initialClock: 0)
				let reactiveLock = ReactiveLock(scheduler: scheduler)
				var subscribed = false

				reactiveLock.acquire()
					.subscribe(onNext: { _ in
						subscribed = true
					})
					.disposed(by: self.disposeBag)

				scheduler.start()
				expect(subscribed).to(beTrue())
			}

			it("acquries the lock only when no one is holding it") {
				let scheduler = TestScheduler(initialClock: 0)
				let reactiveLock = ReactiveLock(scheduler: scheduler)
				var subscribed = false

				reactiveLock.acquire()
					.subscribe()
					.disposed(by: self.disposeBag)

				reactiveLock.acquire()
					.subscribe(onNext: { _ in
						subscribed = true
					})
					.disposed(by: self.disposeBag)

				scheduler.start()
				expect(subscribed).to(beFalse())
			}

			it("lets the consumer access the resource in a FIFO order.") {
				let scheduler = SerialDispatchQueueScheduler(internalSerialQueueName: "ReactiveLockSpec")
				let reactiveLock = ReactiveLock(scheduler: scheduler)
				var subscribed = 0

				let disposable1 = reactiveLock.acquire()
					.subscribe(onNext: { _ in
						subscribed = 1
					})

				let disposable2 = reactiveLock.acquire()
					.subscribe(onNext: { _ in
						subscribed = 2
					})

				reactiveLock.acquire()
					.subscribe(onNext: { _ in
						subscribed = 3
					})
					.disposed(by: self.disposeBag)

				expect(subscribed).toEventually(equal(1))
				disposable1.dispose()
				expect(subscribed).toEventually(equal(2))
				disposable2.dispose()
				expect(subscribed).toEventually(equal(3))
			}
		}

		describe("reactiveLock+rx") {
			it("doesn't subscribe to multiple observable guarded by the same lock with the reactive extension") {
				let scheduler = SerialDispatchQueueScheduler(internalSerialQueueName: "ReactiveLockSpec")
				let reactiveLock = ReactiveLock(scheduler: scheduler)
				let subject1 = PublishSubject<Void>()
				let subject2 = PublishSubject<Void>()
				var subscribed1 = false
				var subscribed2 = false

				subject1
					.asObservable()
					.do(onSubscribe: {
						subscribed1 = true
					})
					.synchronized(using: reactiveLock)
					.subscribe()
					.disposed(by: self.disposeBag)

				subject2
					.asObservable()
					.do(onSubscribe: {
						subscribed2 = true
					})
					.synchronized(using: reactiveLock)
					.subscribe()
					.disposed(by: self.disposeBag)

				expect(subscribed1).toEventually(beTrue())
				expect(subscribed2).to(beFalse())
				subject1.onCompleted()
				expect(subscribed2).toEventually(beTrue())
			}

			it("completes after acquiring the lock and finishing the work") {
				let scheduler = SerialDispatchQueueScheduler(internalSerialQueueName: "ReactiveLockSpec")
				let reactiveLock = ReactiveLock(scheduler: scheduler)
				var completed = false

				Observable.just(())
					.synchronized(using: reactiveLock)
					.do(onCompleted: {
						completed = true
					})
					.subscribe()
					.disposed(by: self.disposeBag)

				expect(completed).toEventually(beTrue())
			}

			it("can early release a lock before entering in the critical section") {
				let dispatchQueue = DispatchQueue(label: "ReactiveLock")
				let scheduler = SerialDispatchQueueScheduler(queue: dispatchQueue, internalSerialQueueName: "ReactiveLock")
				let reactiveLock = ReactiveLock(scheduler: scheduler)
				let subject1 = PublishSubject<Void>()
				let subject2 = PublishSubject<Void>()
				let subject3 = PublishSubject<Void>()
				var subscribed1 = false
				var subscribed2 = false
				var subscribed3 = false
				var disposed3 = false

				subject1
					.asObservable()
					.do(onSubscribe: {
						subscribed1 = true
					})
					.synchronized(using: reactiveLock)
					.subscribe()
					.disposed(by: self.disposeBag)

				subject2
					.asObservable()
					.do(onSubscribe: {
						subscribed2 = true
					})
					.synchronized(using: reactiveLock)
					.subscribe()
					.disposed(by: self.disposeBag)

				let disposable3 = subject3
					.asObservable()
					.do(onSubscribe: {
						subscribed3 = true
					})
					.synchronized(using: reactiveLock)
					.do(onDispose: {
						disposed3 = true
					})
					.subscribe()

				// Check that only one observable is in the critical section
				expect(subscribed1).toEventually(beTrue())
				expect(subscribed2).to(beFalse())
				expect(subscribed3).to(beFalse())

				// Dispose the third subscription and wait until the dispose is dispatched and handled.
				disposable3.dispose()
				dispatchQueue.sync { }

				// Check that the second observable didn't start.
				expect(disposed3).toEventually(beTrue())
				expect(subscribed2).to(beFalse())

				// Complete the first observable and check that the second one has started.
				subject1.onCompleted()
				expect(subscribed2).toEventually(beTrue())
			}
		}
	}
}

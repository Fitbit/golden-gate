//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  OnInterruptedSpec.swift
//  RxbitTests
//
//  Created by Ryan LaSante on 2/13/19.
//

import Nimble
import Quick
import RxBlocking
import RxSwift

// swiftlint:disable trailing_closure function_body_length

class OnInterruptedSpec: QuickSpec {
	override func spec() {
		it("interrupts when disposed without completion") {
			var interrupted = false
			var completed = false
			let subject = PublishSubject<Void>()
			let disposable = subject.asObservable()
				.do(onCompleted: {
					completed = true
				})
				.do(onInterrupted: {
					interrupted = true
				})
				.subscribe()
			disposable.dispose()
			expect(interrupted).to(beTrue())
			expect(completed).to(beFalse())
		}

		it("doesn't interrupt when disposed after completion") {
			var interrupted = false
			var completed = false
			let subject = PublishSubject<Void>()
			let disposable = subject.asObservable()
				.do(onCompleted: {
					completed = true
				})
				.do(onInterrupted: {
					interrupted = true
				})
				.subscribe()
			subject.onCompleted()
			disposable.dispose()
			expect(interrupted).to(beFalse())
			expect(completed).to(beTrue())
		}

		it("interrupts if takeUntil after doOnInterrupted") {
			var interrupted = false
			var completed = false
			let subject = PublishSubject<Void>()
			let disposeSubject = PublishSubject<Void>()
			_ = subject.asObservable()
				.do(onCompleted: {
					completed = true
				})
				.do(onInterrupted: {
					interrupted = true
				})
				.take(until: disposeSubject)
				.subscribe()
			disposeSubject.onNext(())

			expect(interrupted).to(beTrue())
			expect(completed).to(beFalse())
		}

		it("completes if takeUntil before doOnInterrupted") {
			var interrupted = false
			var completed = false
			let subject = PublishSubject<Void>()
			let disposeSubject = PublishSubject<Void>()
			_ = subject.asObservable()
				.take(until: disposeSubject)
				.do(onCompleted: {
					completed = true
				})
				.do(onInterrupted: {
					interrupted = true
				})
				.subscribe()
			disposeSubject.onNext(())

			expect(interrupted).to(beFalse())
			expect(completed).to(beTrue())
		}

		it("interrupts inside subscribe method") {
			var interrupted = false
			var completed = false
			let subject = PublishSubject<Void>()
			let disposable = subject.asObservable()
				.subscribe(onCompleted: {
					completed = true
				}, onInterrupted: {
					interrupted = true
				})
			disposable.dispose()
			expect(interrupted).to(beTrue())
			expect(completed).to(beFalse())
		}
	}
}

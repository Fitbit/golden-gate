//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  IfThenElseSpec.swift
//  Rxbit
//
//  Created by Sylvain Rebaud on 10/29/18.
//

import Nimble
import Quick
import RxSwift

@testable import Rxbit

// swiftlint:disable:next superfluous_disable_command
// swiftlint:disable function_body_length force_try trailing_closure

enum TestError: Error {
	case failed
}

class IfThenElseSpec: QuickSpec {
    override func spec() {
		describe("if(then, else)") {
			var boolSubject: PublishSubject<Bool>!
			var trueSubject: PublishSubject<Int>!
			var falseSubject: PublishSubject<Int>!
			var values = [Int]()
			var lastError: Error?
			var completed: Bool = false
			var disposeBag: DisposeBag!

			beforeEach {
				boolSubject = PublishSubject<Bool>()
				trueSubject = PublishSubject<Int>()
				falseSubject = PublishSubject<Int>()
				values = [Int]()
				lastError = nil
				completed = false
				disposeBag = DisposeBag()

				Observable.if(boolSubject, then: trueSubject, else: falseSubject)
					.subscribe(onNext: {
						values += [$0]
					}, onError: {
						lastError = $0
					}, onCompleted: {
						completed = true
					})
					.disposed(by: disposeBag)
			}

			it("sends no value before a boolean is sent") {
				trueSubject.onNext(1)
				falseSubject.onNext(2)

				expect(values.count) == 0
				expect(lastError).to(beNil())
				expect(completed).to(beFalsy())
			}

			it("send events based on the latest boolean") {
				boolSubject.onNext(true)

				trueSubject.onNext(1)
				falseSubject.onNext(2)
				trueSubject.onNext(3)
				expect(values) == [1, 3]
				expect(lastError).to(beNil())
				expect(completed).to(beFalsy())

				boolSubject.onNext(false)
				trueSubject.onNext(4)
				falseSubject.onNext(5)
				trueSubject.onNext(6)
				expect(values) == [1, 3, 5]
				expect(lastError).to(beNil())
				expect(completed).to(beFalsy())

				trueSubject.onError(TestError.failed)
				expect(lastError).to(beNil())

				falseSubject.onError(TestError.failed)
				expect(lastError).to(matchError(TestError.failed))
				expect(completed).to(beFalsy())
			}

			it("does not complete when only the bool observable completes") {
				boolSubject.onNext(true)
				trueSubject.onNext(1)
				boolSubject.onCompleted()

				expect(values) == [1]
				expect(completed).to(beFalsy())
			}

			it("completes when the bool observable completes and the latest sent observable complete") {
				boolSubject.onNext(true)
				trueSubject.onNext(1)
				boolSubject.onCompleted()
				trueSubject.onCompleted()

				expect(values) == [1]
				expect(completed).to(beTruthy())
			}
		}
    }
}

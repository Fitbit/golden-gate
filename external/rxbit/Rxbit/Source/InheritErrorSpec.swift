//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  InheritErrorSpec.swift
//  RxbitTests
//
//  Created by Bogdan Vlad on 22/01/2019.
//

import Foundation
import Nimble
import Quick
@testable import Rxbit
import RxBlocking
import RxSwift

// swiftlint:disable trailing_closure function_body_length

enum SomeError: Error, Equatable {
	case invalid
}

class InheritErrorSpec: QuickSpec {
	override func spec() {
		let disposeBag = DisposeBag()

		describe("inheritError") {
			it("completes if first observable completes") {
				let first = Observable<Void>.empty()
				let second = Observable<Void>.never()
				var completed = false

				first.inheritError(from: second)
					.subscribe(onCompleted: {
						completed = true
					})
					.disposed(by: disposeBag)

				expect(completed).to(beTrue())
			}

			it("errors if first observable errors") {
				let first = Observable<Void>.error(SomeError.invalid)
				let second = Observable<Void>.never()
				var expectedError: SomeError?

				first.inheritError(from: second)
					.subscribe(onError: { error in
						expectedError = error as? SomeError
					})
					.disposed(by: disposeBag)

				expect(expectedError).to(equal(SomeError.invalid))
			}

			it("errors if second observable errors") {
				let error = SomeError.invalid
				let first = Observable<Void>.never()
				let second = Observable<Void>.error(error)
				var expectedError: SomeError?

				first.inheritError(from: second)
					.subscribe(onError: { error in
						expectedError = error as? SomeError
					})
					.disposed(by: disposeBag)

				expect(expectedError).to(equal(SomeError.invalid))
			}

			it("propagates the elements correctly") {
				let first = Observable.from([1, 2, 3])
				let second = Observable<Void>.never()
				var expectedElements: [Int] = []
				var completed = false

				first.inheritError(from: second)
					.do(onCompleted: {
						completed = true
					})
					.subscribe(onNext: { element in
						expectedElements.append(element)
					})
					.disposed(by: disposeBag)

				expect(expectedElements).toEventually(equal([1, 2, 3]))
				expect(completed).toEventually(beTrue())
			}
		}
	}
}

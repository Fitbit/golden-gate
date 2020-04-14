//
//  DisposeBag+ExtensionsSpec.swift
//  Rxbit
//
//  Created by Sylvain Rebaud on 10/29/18.
//  Copyright © 2018 Fitbit, Inc. All rights reserved.
//

import Nimble
import Quick
import RxSwift

@testable import Rxbit

// swiftlint:disable:next superfluous_disable_command
// swiftlint:disable function_body_length force_try

class DisposeBagExtensionsSpec: QuickSpec {
    override func spec() {
		describe("+=") {
			it("disposes a disposable") {
				let subject = PublishSubject<()>()
				var cancellableDisposable: CompositeDisposable!

				autoreleasepool {
					let disposeBag = DisposeBag()

					let disposable = Observable.never()
						.subscribe(subject)

					cancellableDisposable = CompositeDisposable()
					_ = cancellableDisposable.insert(disposable)
					disposeBag += cancellableDisposable
				}

				expect(cancellableDisposable.isDisposed).to(beTruthy())
			}

			it("disposes a closure") {
				var disposed = false

				autoreleasepool {
					let disposeBag = DisposeBag()

					disposeBag += { disposed = true }
				}

				expect(disposed).to(beTruthy())
			}
		}
    }
}

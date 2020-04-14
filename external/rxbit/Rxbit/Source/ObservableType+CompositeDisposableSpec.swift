//
//  ObservableType+CompositeDisposableSpec.swift
//  Rxbit
//
//  Created by Sylvain Rebaud on 10/29/18.
//  Copyright © 2018 Fitbit, Inc. All rights reserved.
//

import Nimble
import Quick
import RxBlocking
import RxSwift

@testable import Rxbit

// swiftlint:disable:next superfluous_disable_command
// swiftlint:disable function_body_length force_try trailing_closure

class ObservableTypeCompositeDisposableSpec: QuickSpec {
	override func spec() {
		describe("create") {
			it("has a default disposable") {
				let observable = Observable<Bool>.create { observer, _ in
					observer.onNext(true)
				}
				var disposed = false
				var value = false

				autoreleasepool {
					let disposeBag = DisposeBag()
					observable.subscribe(onNext: {
							value = $0
						}, onDisposed: {
							disposed = true
						})
						.disposed(by: disposeBag)
				}

				expect(value) == true
				expect(disposed).to(beTruthy())
			}

			it("disposes disposables") {
				var disposed = false
				var value = false

				let observable = Observable<Bool>.create { observer, disposable in
					observer.onNext(true)

					disposable += {
						disposed = true
					}
				}

				autoreleasepool {
					let disposeBag = DisposeBag()
					observable.subscribe(onNext: {
							value = $0
						})
						.disposed(by: disposeBag)
				}

				expect(value) == true
				expect(disposed).to(beTruthy())
			}
		}
	}
}

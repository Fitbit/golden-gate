//
//  OptionalizeSpec.swift
//  Rxbit
//
//  Created by Marcel Jackwerth on 12/8/17.
//  Copyright © 2017 Fitbit. All rights reserved.
//

import Foundation
import Nimble
import Quick
import RxCocoa
import RxSwift

// swiftlint:disable trailing_closure

class OptionalizeSpec: QuickSpec {
	override func spec() {
		var disposeBag: DisposeBag!

		beforeEach {
			disposeBag = DisposeBag()
		}

		it("makes values in Observables optional") {
			let observable: Observable<Bool> = .just(true)

			waitUntil { done in
				observable.optionalize()
					.subscribe(onNext: {
						expect($0).to(equal(.some(true)))
						done()
					})
					.disposed(by: disposeBag)
			}
		}

		it("makes optionals optional (don't do that)") {
			let observable: Observable<Bool?> = Observable.from([nil, true])

			waitUntil { done in
				observable.optionalize()
					.toArray()
					.subscribe(onSuccess: { array in
						switch array.first {
						case nil??:
							break
						default:
							fail("Expected double-wrapped nil")
						}

						switch array.last {
						case true???:
							break
						default:
							fail("Expected triple-wrapped `true`")
						}

						done()
					})
					.disposed(by: disposeBag)
			}
		}
	}
}

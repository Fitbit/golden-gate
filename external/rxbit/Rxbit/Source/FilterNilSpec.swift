//
//  FilterNilSpec.swift
//  Rxbit
//
//  Created by Sylvain Rebaud on 1/11/18.
//  Copyright © 2018 Fitbit. All rights reserved.
//

import Nimble
import Quick
import RxSwift

@testable import Rxbit

// swiftlint:disable trailing_closure

class FilterNilSpec: QuickSpec {
	override func spec() {
		var disposeBag: DisposeBag!

		beforeEach {
			disposeBag = DisposeBag()
		}

		describe("filterNil") {
			it("unwraps the optional") {
				let _: Observable<Int> = Observable<Int?>
					.just(nil)
					.filterNil()
			}

			it("filters nil values") {
				let values = Observable<Int?>.of(1, nil, 3, 4)
				waitUntil { done in
					values
						.filterNil()
						.toArray()
						.subscribe(onSuccess: {
							expect($0).to(equal([1, 3, 4]))
							done()
						})
						.disposed(by: disposeBag)
				}
			}

			it("filters nil values and keeps types") {
				let values = Observable<Int?>.of(1, nil, 3, 4)
				waitUntil { done in
					values
						.filterNilKeepOptional()
						.toArray()
						.subscribe(onSuccess: {
							expect($0).to(equal([1, 3, 4]))
							done()
						})
						.disposed(by: disposeBag)
				}
			}
		}

	}
}

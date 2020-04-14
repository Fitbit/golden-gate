//
//  NSObject+DisposeBagSpec.swift
//  Rxbit
//
//  Created by Sylvain Rebaud on 8/30/18.
//  Copyright © 2018 Fitbit, Inc. All rights reserved.
//

import Nimble
import Quick
@testable import Rxbit
import RxSwift

class NSObjectDisposeBagSpec: QuickSpec {
	override func spec() {
		let number: NSNumber = 3

		describe("disposeBag") {
			it("is cached") {
				let disposeBag = number.disposeBag
				expect(number.disposeBag) === disposeBag
			}

			it("can be set") {
				waitUntil { done in
					let disposable = Disposables.create {
						done()
					}
					number.disposeBag.insert(disposable)

					// Swap bag which should dispose the previous one
					number.disposeBag = DisposeBag()
				}
			}
		}
	}
}

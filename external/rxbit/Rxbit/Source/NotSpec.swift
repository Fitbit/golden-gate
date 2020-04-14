//
//  NotSpec.swift
//  Rxbit
//
//  Created by Sylvain Rebaud on 10/15/18.
//  Copyright © 2018 Fitbit, Inc. All rights reserved.
//

import Foundation
import Nimble
import Quick
import RxSwift

@testable import Rxbit

class NotSpec: QuickSpec {
	override func spec() {
		describe("not") {
			it("inverts values") {
				let values = Observable.from([false, true, false])
				expect(((try? values.not().toArray().toBlocking().first()) as [Bool]??)) == [true, false, true]
			}
		}
	}
}

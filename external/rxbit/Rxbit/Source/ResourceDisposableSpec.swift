//
//  ResourceDisposableSpec.swift
//  RxbitTests
//
//  Created by Bogdan Vlad on 12/12/2018.
//  Copyright © 2018 Fitbit, Inc. All rights reserved.
//

import Nimble
import Quick
@testable import Rxbit
import RxSwift

private class DummyClass {}

class ResourceDisposableSpec: QuickSpec {
	override func spec() {
		it("keeps the objects alive") {
			var reference: DummyClass? = DummyClass()
			weak var weakReference = reference
			let resourceDisposable = ResourceDisposable(resources: [reference!])
			reference = nil
			expect(weakReference).toNot(beNil())

			resourceDisposable.dispose()
			expect(weakReference).to(beNil())
		}

		it("calls the dispose block") {
			waitUntil { done in
				let resourceDisposable = ResourceDisposable(resources: []) {
					done()
				}

				resourceDisposable.dispose()
			}
		}
	}
}

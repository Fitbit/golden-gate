//
//  FlatMapLatestForwardingNilSpec.swift
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

class FlatMapLatestForwardingNilSpec: QuickSpec {
	override func spec() {
		var disposeBag: DisposeBag!

		beforeEach {
			disposeBag = DisposeBag()
		}

		describe("with a map function with a non-optional result") {
			it("forwards and maps nil") {
				let values = Observable<Int?>.from([10, nil, 20])
				let interval = Observable<Int64>.interval(.milliseconds(50), scheduler: MainScheduler.asyncInstance).take(3)
				let observable = Observable.zip(values, interval).map { tuple in tuple.0 }

				waitUntil { done in
					observable
						.flatMapLatestForwardingNil { value in
							Observable.from([0, 1, 2].map { $0 + value })
						}
						.toArray()
						.subscribe(onSuccess: { array in
							expect(array).to(equal([10, 11, 12, nil, 20, 21, 22]))
							done()
						})
						.disposed(by: disposeBag)
				}
			}
		}

		describe("with a map function with an optional result") {
			it("forwards and maps nil") {
				let values = Observable<Int?>.from([10, nil, 20])
				let interval = Observable<Int64>.interval(.milliseconds(50), scheduler: MainScheduler.asyncInstance).take(3)
				let observable = Observable.zip(values, interval).map { tuple in tuple.0 }

				waitUntil { done in
					observable
						.flatMapLatestForwardingNil { value in
							Observable.from([0, nil, 2].map { $0.map { $0 + value } })
						}
						.toArray()
						.subscribe(onSuccess: { array in
							expect(array).to(equal([10, nil, 12, nil, 20, nil, 22]))
							done()
						})
						.disposed(by: disposeBag)
				}
			}
		}
	}
}
